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

    channel_t * channel_new(nio::selector_t *sel, channel_io_proc cb, void *arg, err::error_t *e)
    {
        void * p = malloc(sizeof(channel_t));
        channel_init((channel_t*)p, sel, cb, arg, e);
        return (channel_t *)p;
    }

    void channel_delete(channel_t * ch) {
        while ( ch->wrops.front ) {
            auto pnode = alg::dlinklist_pop_front(&ch->wrops);
            free(pnode);
        }
        while ( ch->rdops.front ) {
            auto pnode = alg::dlinklist_pop_front(&ch->rdops);
            free(pnode);
        }

        free(ch);
    }


} // end namespace nio
