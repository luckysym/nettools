#include <sym/nio.h>
#include <sym/network.h>

namespace nio {
typedef void (*channel_io_proc)(int event, void *io, void *arg);

const int channel_state_closed  = 0;
const int channel_state_opening = 1;
const int channel_state_open    = 2;
const int channel_state_closing = 3;

typedef struct nio_channel {
    int                 fd;
    int                 state;   // channel_state_*
    selector_t *        sel;
    channel_io_proc     iocb;
} channel_t;

inline 
void channel_event_callback(int fd, int events, void *arg) {
    channel_t * ch = (channel_t *)arg;
    assert("channel_event_callback" == nullptr);
}

inline 
bool channel_init(channel_t * ch, nio::selector_t *sel, channel_io_proc cb, void *arg, err::error_t *e) 
{
    ch->fd = -1;
    ch->sel = sel;
    ch->iocb = cb;
    ch->state = channel_state_closed;
    return true;
}

bool channel_destroy(channel_t *ch, err::error_t *e) 
{
    assert("channel_destroy" == nullptr);
}

bool channel_open_async(channel_t *ch, net::location_t * remote, err::error_t * err) 
{
    assert( ch->fd == -1);
    int fd = net::socket_open_channel(remote, net::sockopt_nonblocked, err);
    if ( fd == -1 ) return false;

    ch->fd = fd;
    bool isok = selector_add(ch->sel, fd, channel_event_callback, ch, err);
    assert( isok );

    isok = selector_request(ch->sel, fd, select_write, -1, err);
    assert(isok);
    return isok;
}

}  // end namespace nio

volatile int G_stop = 0;

void redis_channel_callback(int event, void *io, void *arg) { 
    assert("redis_channel_callback" == nullptr);
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
    
    channel_destroy(&ch, nullptr);
    
    return 0;
}
