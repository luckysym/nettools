#include <sym/nio.h>

namespace nio
{
    bool channel_close_async(channel_t *ch, err::error_t *e) 
    {
        bool isok = selector_request(ch->sel, ch->fd, select_remove, -1, e);
        assert(isok); 
        return isok;
    }

    bool channel_close(channel_t *ch, err::error_t *e) 
    {
        if ( ch->state != channel_state_closed ) {
            ch->state = channel_state_closed;
            bool isok = selector_request(ch->sel, ch->fd, select_remove, -1, e);
            assert(isok);
            return net::socket_close(ch->fd, e);
        }
        return true;
    }
} // end namespace nio
