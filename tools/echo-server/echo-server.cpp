
#include <sym/network.h>
#include <stdio.h>


void listner_event_proc(int fd, int event, void *arg);

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
    net::selector_t * selector = net::selector_create(&err);
    if ( epfd == -1 ) {
        fprintf(stderr, "[error] failed to create selector, %s", err->str);
        net::free_error_info(&err);
        net::socket_close(sfd, nullptr);
        return -1;
    }

    // add listener to selector
    bool isok = net::selector_add(selector, sfd, listner_event_proc, nullptr, &err);
    if ( !isok ) {
        fprintf(stderr, "[error] failed to create selector, %s", err->str);
        net::free_error_info(&err);
        net::socket_close(sfd, nullptr);
        return -1;
    }

    while (1) {
        // request accept event for no timeout
        int revents = net::select_read;
        isok = net::selector_request(selector, sfd, revents, -1, &err);
        if ( !isok ) {
            fprintf(stderr, "[error] failed to create selector, %s", err->str);
            net::free_error_info(&err);
            break;
        }   
    }

    net::selector_destroy(selector, nullptr);
    net::socket_close(sfd, nullptr);

    return 0;
}

void listner_event_proc(int fd, int event, void *arg)
{
    return ;
}