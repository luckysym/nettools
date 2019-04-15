#include <sym/nio.h>
#include <sym/network.h>
#include <sym/io.h>

namespace nio {

bool channel_shutdown(channel_t *ch, int how, err::error_t *e)
{
    return net::socket_shutdown(ch->fd, how, e);
}

inline 
void channel_event_callback(int fd, int events, void *arg) {
    channel_t * ch = (channel_t *)arg;
    err::error_t err;
    err::init_error_info(&err);

    if ( events == select_write ) {
        if ( ch->state == channel_state_opening ) {
            ch->state = channel_state_open;
            ch->iocb(ch, channel_event_connected, nullptr, arg);
        } else {
            assert("channel_event_callback write state" == nullptr);
        }
    }
    else if ( events == select_read ) {
        buffer_node_t *pn = ch->rdbufs.front;
        int n = 0;
        int t = 0;
        int len =  pn->value.buf.size - pn->value.buf.end;

        do {
            char * ptr = pn->value.buf.data + pn->value.buf.end + t;
            int sz = pn->value.buf.size - pn->value.buf.end - t;
            n = net::socket_recv(ch->fd, ptr, sz, &err);
            if ( n > 0 ) t += n;
        } while ( n > 0 && t < len);

        pn->value.buf.end += t;  // 总收到t个字节，end游标后移
        
        // 全部收到，或者没有指定必须收到指定长度，就执行回调，否则继续监听收事件
        if ( pn->value.buf.end == pn->value.buf.size || !(pn->value.flags & nio_flag_exact_size) ) {
            pn = alg::dlinklist_pop_front(&ch->rdbufs);
            ch->iocb(ch, channel_event_received, &pn->value.buf, ch->arg);
            free(pn);
        }

        /// 如果接收缓存队列里还有需要接收消息的缓存，继续请求读事件。
        if ( ch->rdbufs.front ) {
            bool isok = selector_request(ch->sel, ch->fd, select_read, pn->value.exp, &err);
            assert(isok);
        }
    } 
    else if ( events == select_remove ) {
        // channel will be closed
        bool isok = net::socket_close(ch->fd, &err);
        assert(isok);
        ch->iocb(ch, channel_event_closed, nullptr, ch->arg);
    }
    else if ( events == select_add ) {
        // added
    }
    else {
        assert("channel_event_callback event" == nullptr);
    }
}

inline 
bool channel_init(channel_t * ch, nio::selector_t *sel, channel_io_proc cb, void *arg, err::error_t *e) 
{
    ch->fd = -1;
    ch->sel = sel;
    ch->iocb = cb;
    ch->arg  = arg;
    ch->state = channel_state_closed;

    alg::dlinklist_init(&ch->wrbufs);
    alg::dlinklist_init(&ch->rdbufs);
    return true;
}

bool channel_close_async(channel_t *ch, err::error_t *e) 
{
    bool isok = selector_request(ch->sel, ch->fd, select_remove, -1, e);
    assert(isok); 
    return isok;
}

bool channel_close(channel_t *ch, err::error_t *e) 
{
    bool isok = selector_request(ch->sel, ch->fd, select_remove, -1, e);
    assert(isok);
    return net::socket_close(ch->fd, e);
}

bool channel_open_async(channel_t *ch, net::location_t * remote, err::error_t * err) 
{
    assert( ch->fd == -1);
    int fd = net::socket_open_stream(remote, net::sockopt_nonblocked, err);
    if ( fd == -1 ) return false;

    ch->fd = fd;
    ch->state = channel_state_opening;
    bool isok = selector_add(ch->sel, fd, channel_event_callback, ch, err);
    assert( isok );

    isok = selector_request(ch->sel, fd, select_write, -1, err);
    assert(isok);
    return isok;
}

bool channel_sendn_async(channel_t *ch, const char * buf, int len, int64_t exp, err::error_t * err) {

    // try to send first
    int sr = 0;
    int total  = 0;
    do {
        sr = net::socket_send(ch->fd, buf + total, len, err );
        if ( sr > 0 ) total += sr;
    } while ( sr > 0 && total < len);

    SYM_TRACE_VA("channel_sendn_async, %d / %d sent", total, len);

    // send ok, do callback, or wait for writable
    if ( total == len ) {
        io::buffer_t obuf;
        obuf.data = (char *)buf;
        obuf.begin = obuf.end = obuf.size = len; 
        ch->iocb(ch, channel_event_sent, &obuf, ch->arg);
        return true;
    } else {

        // create a buffer node and push to channel write buffer queue
        buffer_node_t * pn = (buffer_node_t*)malloc(sizeof(buffer_node_t));
        io::buffer_init(&pn->value.buf);
        pn->prev = pn->next = nullptr;

        pn->value.buf.data = (char *)buf;
        pn->value.buf.size = len;
        pn->value.buf.begin = total;
        pn->value.buf.end  = len;

        pn->value.flags = nio_flag_exact_size;
        pn->value.exp = exp;
    
        alg::dlinklist_push_back(&ch->wrbufs, pn);
        bool isok = selector_request(ch->sel, ch->fd, select_write, exp, err);
        return isok;
    }
}

bool channel_recvsome_async(channel_t *ch, char * buf, int len, int64_t exp, err::error_t * err) 
{
    // try to send first
    int rr = 0;
    int total  = 0;

    do {
        rr = net::socket_recv(ch->fd, buf + total, len, err );
        if ( rr > 0 ) total += rr;
    } while ( rr > 0 && total < len );

    SYM_TRACE_VA("channel_recvsome_async, %d / %d sent", total, len);

    // readsome只要收到数据就返回，一个没收到就selecting
    // 所以只要收到数据或者出错，直接回调；没有收到数据，就申请select
    if ( total > 0 || rr < 0 ) {
        io::buffer_t ibuf;
        ibuf.data = (char *)buf;
        ibuf.begin = 0;
        ibuf.end = total;
        ibuf.size = len;

        int event = channel_event_received;
        if ( rr < 0 ) event |= channel_event_error;
        ch->iocb(ch, event, &ibuf, ch->arg);

        return true;
    } else {
        // create a buffer node and push to channel read buffer queue
        buffer_node_t * pn = (buffer_node_t*)malloc(sizeof(buffer_node_t));
        io::buffer_init(&pn->value.buf);
        pn->prev = pn->next = nullptr;

        pn->value.buf.data = (char *)buf;
        pn->value.buf.size = len;
        pn->value.buf.begin = total;
        pn->value.buf.end  =  0;
        pn->value.flags = 0;
        pn->value.exp = exp;
    
        alg::dlinklist_push_back(&ch->rdbufs, pn);
        bool isok = selector_request(ch->sel, ch->fd, select_read, exp, err);
        return isok;
    }
}

}  // end namespace nio

volatile int G_stop = 0;

int G_seq = 0;

void redis_channel_callback(nio::channel_t *ch, int event, void *io, void *arg) { 
    bool isok;
    err::error_t err;
    err::init_error_info(&err);

    if ( event == nio::channel_event_connected ) {
        fprintf(stderr, "connected \n");
        const char *cmd = "set abc 123456\r\n";
        isok = nio::channel_sendn_async(ch, cmd, strlen(cmd), -1, &err);
        assert(isok);
    } 
    else if ( event == nio::channel_event_sent ) {
        char * buf = (char *)malloc(128);
        isok = nio::channel_recvsome_async(ch, buf, 128, -1, &err);
        assert(isok);
    } 
    else if ( event == nio::channel_event_received ) {
        io::buffer_t * buf = (io::buffer_t*)io;
        assert(buf);
        fprintf(stderr, "%s", buf->data);
        free(buf->data);
        G_seq += 1;
        if ( G_seq == 1) {
            const char *cmd = "get abc\r\n";
            isok = nio::channel_sendn_async(ch, cmd, strlen(cmd), -1, &err);
            assert(isok);
        } else {
            G_stop = 1;
            nio::selector_notify(ch->sel, &err);
        }
    }
    else {
        assert("redis_channel_callback" == nullptr);
    }
}

int main(int argc, char **argv)
{
    using namespace nio;

    bool isok;
    
    if ( argc < 2 ) {
        fprintf(stderr, "usage: %s <url>\n", argv[0]);
        return -1;
    }

    net::location_t remote;
    net::location_init(&remote);

    auto p = net::location_from_url(&remote, argv[1]);
    assert(p);
    
    fprintf(stderr, "location: %s %s:%d\n", remote.proto, remote.host, remote.port);

    nio::selector_t selector;
    isok = nio::selector_init(&selector, 0, nullptr);
    assert(isok);

    channel_t ch;
    isok = channel_init(&ch, &selector, redis_channel_callback, nullptr, nullptr);
    assert(isok);

    isok = channel_open_async(&ch, &remote, nullptr);
    assert(isok);

    while ( !G_stop ) {
        isok = nio::selector_run(&selector, nullptr);
        assert(isok);
    }
    
    channel_shutdown(&ch, channel_shut_both, nullptr);
    channel_close(&ch, nullptr);
    
    return 0;
}
