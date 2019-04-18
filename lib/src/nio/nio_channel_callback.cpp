#include <sym/nio.h>

namespace nio
{
namespace detail {
     
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
            detail::nio_oper_node_t *pn = ch->rdops.front;
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
                pn = alg::dlinklist_pop_front(&ch->rdops);
                ch->iocb(ch, channel_event_received, &pn->value.buf, ch->arg);
                free(pn);
            }

            /// 如果接收缓存队列里还有需要接收消息的缓存，继续请求读事件。
            if ( ch->rdops.front ) {
                bool isok = selector_request(ch->sel, ch->fd, select_read, pn->value.exp, &err);
                assert(isok);
            }
        } 
        else if ( events == select_remove ) {
            // channel will be closed
            if ( ch->state != channel_state_closed ) {
                ch->state = channel_state_closed;
                bool isok = net::socket_close(ch->fd, &err);
                assert(isok);
            }
            ch->iocb(ch, channel_event_closed, nullptr, ch->arg);
        }
        else if ( events == select_add ) {
            // added
        }
        else if ( events == select_error ) {
            if ( ch->state == channel_state_opening ) {
                // failed to open socket
                int event = channel_event_connected | channel_event_error;
                ch->state = channel_state_closed;
                ch->iocb(ch, event, nullptr, ch->arg);
            } else {
                assert("channel_event_callback error" == nullptr);
            }
        }
        else {
            assert("channel_event_callback event" == nullptr);
        }
    }

} // end namespace detail
} // end namespace nio