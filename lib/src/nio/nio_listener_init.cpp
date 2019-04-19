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

        isok = selector_request(lis->sel, fd, select_read, -1, e);
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

} // end namespace nio
