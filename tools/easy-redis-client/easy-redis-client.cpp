#include <sym/nio.h>
#include <sym/network.h>
#include <sym/io.h>

volatile int G_stop = 0;

int G_seq = 0;

void redis_channel_callback(nio::channel_t *ch, int event, void *io, void *arg) { 
    bool isok;
    err::error_t err;
    err::init_error_info(&err);

    if ( event == nio::channel_event_connected ) {
        fprintf(stderr, "connected \n");
        const char *cmd = "set abc 123456\r\n";
        isok = nio::channel_sendn_async(ch, cmd, strlen(cmd), -1, &err);
        assert(isok);
    } 
    else if ( event == nio::channel_event_sent ) {
        char * buf = (char *)malloc(128);
        isok = nio::channel_recvsome_async(ch, buf, 128, -1, &err);
        assert(isok);
    } 
    else if ( event == nio::channel_event_received ) {
        io::buffer_t * buf = (io::buffer_t*)io;
        assert(buf);
        fprintf(stderr, "%s", buf->data);
        free(buf->data);
        G_seq += 1;
        if ( G_seq == 1) {
            const char *cmd = "get abc\r\n";
            isok = nio::channel_sendn_async(ch, cmd, strlen(cmd), -1, &err);
            assert(isok);
        } else {
            G_stop = 1;
            nio::selector_notify(ch->sel, &err);
        }
    }
    else {
        assert("redis_channel_callback" == nullptr);
    }
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
    
    fprintf(stderr, "location: %s %s:%d\n", remote.proto, remote.host, remote.port);

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
    
    channel_shutdown(&ch, channel_shut_both, nullptr);
    channel_close(&ch, nullptr);
    
    return 0;
}
