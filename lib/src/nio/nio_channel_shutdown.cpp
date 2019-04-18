#include <sym/nio.h>

namespace nio
{
    bool channel_shutdown(channel_t *ch, int how, err::error_t *e)
    {
        return net::socket_shutdown(ch->fd, how, e);
    }
} // end namespace nio
