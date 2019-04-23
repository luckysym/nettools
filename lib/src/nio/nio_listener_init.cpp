#include <sym/nio.h>

namespace nio 
{
    bool listener_init(listener_t *lis, selector_t *sel, listener_io_proc cb, void *arg)
    {
        lis->fd = -1;
        lis->state = listener_state_closed;
        lis->sel = sel;
        lis->iocb = cb;
        lis->arg = arg;

        return true;
    }

    bool listener_open(listener_t * lis, net::location_t * local, err::error_t *e) 
    {
        assert(lis->fd == -1);
 
        int lisopt = net::sockopt_nonblocked  | net::sockopt_reuseaddr;
        int fd = net::socket_open_listener(local, lisopt, e);
        if ( fd == -1 ) return false;

        lis->state = listener_state_open;
        lis->fd = fd;
        bool isok = selector_add(lis->sel, fd, detail::listener_event_callback, lis, e);
        assert(isok);

        return true;
    }

    bool listener_close(listener_t * lis, err::error_t *e)
    {
        if ( lis->state != listener_state_closed) {
            lis->state = listener_state_closed;
            bool isok = selector_remove(lis->sel, lis->fd, e);
            return net::socket_close(lis->fd, e);
        }
        return true;
    }

    bool listener_accept_async(listener_t * lis, channel_t * ch, err::error_t *e)
    {
        return selector_request(lis->sel, lis->fd, select_read, -1, e);
    }

} // end namespace nio

namespace nio {
namespace detail {

    void listener_event_callback(int sfd, int events, void *arg)
    {
        assert( sfd >= 0 );

        nio_listener * lis = (nio_listener*)arg;
        assert( lis );

        err::error_t err;
        err::init_error_info(&err);

        if ( events == nio::select_read ) {
            net::location_t remote;
            net::location_init(&remote);

            while ( lis->chops.front ) {
                channel_node_t *chn = lis->chops.front;
                channel_t *ch = chn->value;
                
                int cfd = net::socket_accept(sfd, net::sockopt_nonblocked|net::sockopt_tcp_nodelay, &remote, &err);
                if ( cfd >= 0 ) {
                    ch->fd = cfd;
                    ch->state = channel_state_open;

                    listen_io_param_t ioparam;
                    ioparam.channel = ch;
                    ioparam.remote = &remote;
                    lis->iocb(lis, listener_event_accepted, &ioparam, lis->arg);
                    
                    // pop and free the channel node
                    alg::dlinklist_pop_front(&lis->chops);
                    free(chn);

                    // destroy location
                    net::location_free(&remote);
                } else if ( err.str == nullptr ) {
                    // no more channel to accept
                    break;  
                } else {
                    // accept error
                    listen_io_param_t ioparam;
                    ioparam.channel = ch;
                    ioparam.remote = nullptr;
                    lis->iocb(lis, listener_event_error, &ioparam, lis->arg);

                    // pop and free the channel node
                    alg::dlinklist_pop_front(&lis->chops);
                    free(chn);
                }
            }
        } else {
            assert("listener_event_callback unknown events" == nullptr);
        }
    }

} } // end namespace nio::detail