
#include <sym/network.h>
#include <stdio.h>
#include <assert.h>

void listner_event_proc(net::selector_t * sel, int fd, int event, void *arg);
void channel_event_proc(net::selector_t * sel, int fd, int event, void *arg);


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
    net::selector_t * selector = net::selector_create(0, &err);
    if ( !selector ) {
        fprintf(stderr, "[error] failed to create selector, %s", err.str);
        net::free_error_info(&err);
        net::socket_close(sfd, nullptr);
        return -1;
    }

    // add listener to selector
    bool isok = net::selector_add(selector, sfd, listner_event_proc, nullptr, &err);
    if ( !isok ) {
        fprintf(stderr, "[error] failed to create selector, %s", err.str);
        net::free_error_info(&err);
        net::selector_destroy(selector, nullptr);
        net::socket_close(sfd, nullptr);
        return -1;
    }

    // request accept event for no timeout
    int revents = net::select_accept;
    isok = net::selector_request(selector, sfd, revents, -1, &err);
    if ( !isok ) {
        fprintf(stderr, "[error] failed to create selector, %s", err.str);
        net::free_error_info(&err);
        net::selector_destroy(selector, nullptr);
        net::socket_close(sfd, nullptr);
        return -1;
    }   
    
    int r = net::selector_run(selector, &err);    
    if ( r < 0 ) {
        fprintf(stderr, "[error] failed to run selector, %s", err.str);
        net::free_error_info(&err);
    }

    net::selector_destroy(selector, nullptr);
    net::socket_close(sfd, nullptr);

    return 0;
}

// 监听socket事件回调
void listner_event_proc(net::selector_t * sel, int fd, int event, void *arg)
{
    if ( event == net::select_accept ) {
        net::location_t remote;
        net::error_t    err;

        net::location_init(&remote);
        net::init_error_info(&err);
        while (1) {
            int cfd = net::socket_accept_channel(fd, &remote, &err);
            if ( cfd == -1 && err.str) {
                fprintf(stderr, "[error] failed to accept channel, %s\n", err.str);
                net::free_error_info(&err);
                net::selector_remove(sel, fd);
            } else if ( cfd == -1 && err.str == nullptr ) {
                // no more channel
                break;
            } else {
                fprintf(stderr, "[info] new client accept, fd:%d, %s:%d\n", cfd, remote.host, remote.port);
                net::location_free(&remote);

                bool isok = net::selector_add(sel, cfd, channel_event_proc, nullptr, &err);
                assert(isok);
                isok = net::selector_request(sel, cfd, net::select_read, -1, &err);
                assert(isok);
            }
        } // end while  
    } else if ( event == net::select_remove) {
        fprintf(stderr, "[info] listener will be closed, fd:%d\n", fd);
        net::socket_close(fd, nullptr);
    }
    return ;
}

