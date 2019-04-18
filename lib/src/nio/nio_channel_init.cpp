#include <sym/nio.h>

namespace nio
{
    bool channel_init(channel_t * ch, nio::selector_t *sel, channel_io_proc cb, void *arg, err::error_t *e) 
    {
        ch->fd = -1;
        ch->sel = sel;
        ch->iocb = cb;
        ch->arg  = arg;
        ch->state = channel_state_closed;

        alg::dlinklist_init(&ch->wrops);
        alg::dlinklist_init(&ch->rdops);
        return true;
    }


} // end namespace nio
