#include <sym/srpc.h>
#include <assert.h>
#include <map>

#define LOCAL_URL "0.0.0.0:40001"


void on_accept(nio::listener_t *lis, int event, const nio::listen_io_param_t *io, void *arg);
void on_rpc_receive(srpc::connection_t * conn, int status, io::buffer_t * buf, void *arg);
void on_rpc_send(srpc::connection_t * conn, int status, io::buffer_t * buf, void *arg);
void on_rpc_close(srpc::connection_t * conn, int status, void *arg) ;

int main(int argc, char **argv)
{
    using namespace srpc;

    const char * local_url = LOCAL_URL;
    if ( argc > 1 ) local_url = argv[1];
    bool isok ;

    err::error_t err;
    err::init_error_info(&err);

    std::map<int, nio::channel_t *> channels;  // accepted channel list

    // 设置本地地址
    net::location_t clocal;
    net::location_init(&clocal);
    net::location_from_url(&clocal, local_url);

    // 初始化selector
    nio::selector_t selector;
    nio::selector_init(&selector, 0, &err);

    // 初始化并创建listener
    nio::listener_t listener;
    nio::listener_init(&listener, &selector, on_accept, &channels);
    nio::listener_open(&listener, &clocal, &err);

    // 创建并初始化一个channel，并执行异步接收新连接.
    nio::channel_t *channel = (nio::channel_t *)malloc(sizeof(nio::channel_t));
    nio::channel_init(channel, &selector, nullptr, nullptr, &err);
    nio::listener_accept_async(&listener, channel, &err);

    // 执行select run, 等待并处理相关事件。
    while ( 1 ) {
        int r = nio::selector_run(&selector, &err);
    }

    return 0;
}

void on_accept(nio::listener_t *lis, int event, const nio::listen_io_param_t *io, void *arg)
{
    std::map<int, nio::channel_t*> *chs = (std::map<int, nio::channel_t*> *)arg;

    err::error_t err;
    err::init_error_info(&err);

    if ( event == nio::listener_event_accepted ) {
        SYM_TRACE_VA("on_accepted, accept new channel %d, remote %s:%d", 
            io->channel->fd, io->remote->host, io->remote->port); 

        (*chs)[io->channel->fd] = io->channel;

        // create new rpc connection, and wait for message arrival
        srpc::connection_cb_t cbs { on_rpc_receive, on_rpc_send, on_rpc_close, nullptr };
        
        srpc::connection_t * conn = srpc::connection_new(io->channel, &cbs, &err);
        io::buffer_t rbuf; 
        io::buffer_alloc(&rbuf, 1024);
        srpc::receive_async(conn, &rbuf, -1, &err);

        // accept another channel
        nio::channel_t * ch = nio::channel_new(io->channel->sel, nullptr, nullptr, &err);
        nio::listener_accept_async(lis, ch, &err);
    } else {
        SYM_TRACE_VA("on_accepted, unknwon event %d", event);
    }
}

void on_rpc_receive(srpc::connection_t * conn, int status, io::buffer_t * buf, void *arg)
{
    SYM_TRACE_VA("on_rpc_receive, status %d", status);
}

void on_rpc_send(srpc::connection_t * conn, int status, io::buffer_t * buf, void *arg) 
{
    SYM_TRACE_VA("on_rpc_send, status %d", status);
}

void on_rpc_close(srpc::connection_t * conn, int status, void *arg) 
{
        SYM_TRACE_VA("on_rpc_close, status %d", status);
}