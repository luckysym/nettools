#include <sym/nio.h>

namespace nio
{
    bool channel_open_async(channel_t *ch, net::location_t * remote, err::error_t * err) 
    {
        assert( ch->fd == -1);
        err::error_t e;
        err::init_error_info(&e);
        int fd = net::socket_open_stream(remote, net::sockopt_nonblocked, &e);
        if ( fd == -1 ) {
            // connect failed
            SYM_TRACE_VA("connect failed: %s", e.str);
            if ( err ) err::move_error_info(err, &e);
            else err::free_error_info(&e);
            return false;
        } else if ( e.str != nullptr ) {
            // connect in progress
            ch->state = channel_state_opening;
            err::free_error_info(&e);
            SYM_TRACE("connect in progress");
        } else {
            // channel open succeeded
            ch->state = channel_state_open;
        }

        ch->fd = fd;
        bool isok = selector_add(ch->sel, fd, detail::channel_event_callback, ch, err);
        assert( isok );

        if ( ch->state == channel_state_opening ) {
            isok = selector_request(ch->sel, fd, select_write, -1, err);
            assert(isok);
        }
        return isok;
    }
} // end namespace nio
