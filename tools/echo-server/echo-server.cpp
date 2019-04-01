
#include <sym/network.h>
#include <sym/io.h>
#include <stdio.h>
#include <assert.h>

void listner_event_proc(int fd, int event, void *arg);
void channel_event_proc(int fd, int event, void *arg);

struct echo_channel 
{
    int               fd;
    net::location_t   remote;
    io::buffer_t      rdbuf;
    net::selector_t * selector;
};

/**
 * command:  echosvr <remote_url> <message> 
 * example:  echosvr tcp://*:8989
 */
int main(int argc, char **argv)
{
    if ( argc < 3 ) {
        fprintf(stderr, "help: %s <remote_url> <message>\n", argv[0]);
        return 0;
    }

    char * remote = argv[1];
    char * message = argv[2];

    // remote url to net::location
    net::location loc;
    net::location_init(&loc);
    net::location * p = net::location_from_url(&loc, remote);
    if ( !p ) {
        fprintf(stderr, "[error] bad url for pasing, %s\n", remote);
        return -1;
    }
    struct net::error_info err;
    net::init_error_info(&err);
    
    // open socket listener
    int opts = net::sockopt_reuseaddr | net::sockopt_nonblocked;
    int sfd = net::socket_open_listener(&loc, opts, &err);
    if ( sfd == -1 ) {
        fprintf(stderr, "[error] failed to open socket listener, %s\n", err.str);
        net::free_error_info(&err);
        return -1;
    }
    sleep(1);

    // create selector 
    net::selector_t selector ;
    auto sel = net::selector_init(&selector, 0, &err);
    if ( !sel ) {
        fprintf(stderr, "[error] failed to create selector, %s", err.str);
        net::free_error_info(&err);
        net::socket_close(sfd, nullptr);
        return -1;
    }

    // add listener to selector
    bool isok = net::selector_add(sel, sfd, listner_event_proc, &sel, &err);
    if ( !isok ) {
        fprintf(stderr, "[error] failed to create selector, %s", err.str);
        net::free_error_info(&err);
        net::selector_destroy(sel, nullptr);
        net::socket_close(sfd, nullptr);
        return -1;
    }
    
    int r = net::selector_run(sel, &err);    
    if ( r < 0 ) {
        fprintf(stderr, "[error] failed to run selector, %s", err.str);
        net::free_error_info(&err);
    }

    net::selector_destroy(sel, nullptr);
    net::socket_close(sfd, nullptr);

    return 0;
}

// 监听socket事件回调
void listner_event_proc(int fd, int event, void *arg)
{
    net::selector_t *sel = (net::selector_t*)arg;
    assert(sel);

    if ( event == net::select_accept ) {
        net::location_t remote;
        net::error_t    err;

        net::location_init(&remote);
        net::init_error_info(&err);
        while (1) {
            int cfd = net::socket_accept(fd, &remote, &err);
            if ( cfd == -1 && err.str) {
                fprintf(stderr, "[error] failed to accept channel, %s\n", err.str);
                net::free_error_info(&err);
                net::selector_remove(sel, fd, &err);
            } else if ( cfd == -1 && err.str == nullptr ) {
                // no more channel
                net::location_free(&remote);
                break;
            } else {
                fprintf(stderr, "[info] new client accept, fd:%d, %s:%d\n", cfd, remote.host, remote.port);
                
                echo_channel *ch = new echo_channel;
                memset(ch, 0, sizeof(echo_channel));
                ch->fd = cfd;
                ch->selector = sel;
                net::location_copy(&ch->remote, &remote);
                io::buffer_alloc(&ch->rdbuf, 256);
                
                bool isok = net::selector_add(sel, cfd, channel_event_proc, ch, &err);
                assert(isok);
                net::location_free(&remote);
            }
        } // end while  
    } else if ( event == net::select_remove) {
        fprintf(stderr, "[info] listener will be closed, fd:%d\n", fd);
        net::socket_close(fd, nullptr);
    } else if ( event == net::select_add ) {
        // request accept event for no timeout
        net::error_t err;
        net::init_error_info(&err);
        int revents = net::select_accept;
        bool isok = net::selector_request(sel, fd, revents, -1, &err);
        assert( !isok );
    }
    return ;
}

void channel_event_proc(int fd, int event, void *arg)
{
    net::error_t err;
    net::init_error_info(&err);

    echo_channel * ch = (echo_channel *)arg;
    assert(ch);

    net::selector_t *sel = ch->selector;
    assert(sel);

    if ( event == net::select_read ) {
        while (1) {
            int    remain = 0;
            
            if ( ch->rdbuf.data ) {
                io::buffer_pullup(&ch->rdbuf);  // 把缓存中间的数据移到缓存头部
                remain = ch->rdbuf.size - ch->rdbuf.end;
            }
            if ( remain == 0 ) {
                io::buffer_realloc(&ch->rdbuf, ch->rdbuf.size + 32);
                remain = ch->rdbuf.size - ch->rdbuf.end;
            } 
            char * ptr = ch->rdbuf.data + ch->rdbuf.end;
            
            // receive data
            int r = net::socket_recv(ch->fd, ptr, remain, &err);
            if ( r > 0 ) {
                // data received
                ch->rdbuf.end += r;

                // request write
                bool sok = net::selector_request(sel, fd, net::select_write, -1, &err);
                assert(sok);
            } else if ( r == 0 ) {
                // nothing received
                break;
            } else {
                // error, or remote closed
                fprintf(stderr, "[info] channel recv failed, fd:%d\n", fd);
                bool sok = net::selector_remove(sel, fd, &err);
                assert(sok);
            }
        }
    } else if ( event == net::select_write ) {
        while ( 1 ) {
            char * ptr = ch->rdbuf.data + ch->rdbuf.begin;
            int    remain = ch->rdbuf.end - ch->rdbuf.begin;
            
            if ( remain == 0 ) {
                // all sent, receive next
                bool sok = net::selector_request(sel, fd, net::select_read, -1, &err);
                assert(sok);
                break; 
            }

            int s = net::socket_send(ch->fd, ptr, remain, &err);
            if ( s > 0 ) {
                // sent some ok
                ch->rdbuf.begin += s;
                
            } else if ( s == 0 ) {
                // no data sent
                break;
            } else {
                // send error
                fprintf(stderr, "[info] channel send failed, fd:%d\n", fd);
                bool sok = net::selector_remove(sel, fd, &err);
                assert(sok);
            }
        }
    } else if ( event == net::select_remove) {

        fprintf(stderr, "[info] channel will be closed, fd:%d\n", fd);

        net::socket_close(ch->fd, nullptr);
        io::buffer_free(&ch->rdbuf);
        net::location_free(&ch->remote);

        ch->fd = -1;
    } else if ( event == net::select_add ) {
        
        // channel added, wait for reading
        bool sok = net::selector_request(sel, fd, net::select_read, -1, &err);
        assert(sok);
        
    } else if ( event == net::select_error ) {
        fprintf(stderr, "[info] channel wait error, fd:%d\n", fd);
        bool sok = net::selector_remove(sel, fd, &err);
        assert(sok);
    } else {
        assert("bad select event" == nullptr);
    }
}