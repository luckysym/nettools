
#include <sym/network.h>
#include <stdio.h>

/**
 * command:  echocli <remote_url> <message> 
 * example:  echocli tcp://192.168.1.1:8989 "hello, world"
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
    
    // open socket channel
    int opts = net::sockopt_tcp_nodelay | net::sockopt_linger | net::sockopt_nonblocked;
    
    int fd = net::socket_open_channel(&loc, opts, &err);
    if ( fd == -1 ) {
        fprintf(stderr, "[error] failed to open socket channel, %s\n", err.str);
        net::free_error_info(&err);
        return -1;
    }
    sleep(1);

    // send message
    int len = strlen(message);
    int bytes = net::socket_channel_sendn(fd, message, len, 5000, &err);
    if ( bytes != len ) {
        fprintf(stderr, "[error] failed to send message, fd: %d, len: %d, sent: %d, %s\n", fd, len, bytes, err.str);
        net::free_error_info(&err);
        net::socket_close(fd, nullptr);
        return -1;
    }

    // receive message
    char * rbuf = new char[len + 1];
    bytes = net::socket_channel_recvn(fd, rbuf, len, 5000, &err);
    if ( bytes != len ) {
        fprintf(stderr, "[error] failed to recv message, fd: %d, len: %d, received: %d, %s\n", fd, len, bytes, err.str);
        net::free_error_info(&err);
        net::socket_close(fd, nullptr);
        return -1;
    }

    // print received message and close socket
    rbuf[bytes - 1] = '\0';
    printf("message received: \n%s\n", rbuf);
    net::socket_close(fd, nullptr);
    net::location_free(&loc);

    return 0;
}
