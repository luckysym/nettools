
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace net {
    
    struct error_info {
        const char * str;
    };

    struct socket_attrib {
        int af;
        int type;
        int proto;
    };

    struct location {
        char * protocol;
        char * host;
        int    port;
        char * path;
    };

    /// gets and returns current timestamp in microseconds from epoch-time 
    int64_t now();

    /// parse url to location, return the localtion pointer if success, null if failed.
    struct location * location_from_url(struct location * loc, const char * url);

    /// free location members
    void location_free(struct localtion * loc);

    /// free error info members
    void free_error_info(struct error_info * err);

    /// 根据协议名称设置socket属性参数
    /// \param attr out, 输出的socket参数。
    /// \param name in, 名称，如tcp, tcp6, udp, udp6, unix
    /// \return 解析成功，返回socket_attrib结构指针，否则返回null
    socket_attrib * sockattr_from_protocol(socket_attrib * attr, const char * name);

    /// send message to remote, and return if all data sent or error or timeout, 
    /// returns total bytes sent if succeeded, or return -1 if failed, or 0 if timeout .
    int socket_channel_sendn(int fd, const char *data, int len, int timeout, error_info * err);

    /// receive message from remote, returns total bytes received if data buffer is full, 
    /// or return -1 if failed, or 0 if timeout.
    int socket_channel_recvn(int fd, char *data, int len, int timeout, error_info *err);

    /// create and open a remote socket channel to the remote location, and returns the socket fd.
    /// returns -1 if failed.
    int socket_open_channel(const location *remote, error_info * err);

    /// close socket fd. returns true if success, 
    bool socket_close(int fd, struct error_info *err);
} // end namespace net 

inline 
void net::free_error_info(net::error_info * err)
{
    if ( err->str ) {
        free((void *)err->str);
        err->str = nullptr;
    }
}

inline 
int net::socket_channel_sendn(int fd, const char *data, int len, int timeout, net::error_info * err)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    int64_t now = net::now();
    int64_t exp = now + (int64_t)timeout * 1000;
    int pos = 0;

    while ( timeout != 0 ) {
        int r = poll(&pfd, 1, timeout);
        if ( r > 0 ) {
            if ( pfd.revents & POLLIN) {
                ssize_t n = ::send(fd, data + pos, len - pos, 0);
                if ( n >= 0) {
                    pos += n;
                } else {
                    err->str = strdup("failed to send data");
                    return -1;
                }
                if ( pos == len ) return len;
            } else {
                err->str = strdup("writable event wait failed");
                return -1;
            }
        } else if ( r == 0 ) {
            break;  // timeout
        } else {
            if ( errno != EINTR ) {
                char errbuf[256];
                snprintf(errbuf, 256, "wait failed, %s", strerror(errno));
                err->str = strdup(errbuf);
                return -1;  // failed
            }
        }
        now = net::now();
        if ( exp - now > 0 ) {
            timeout = (int)((exp - now) / 1000);
        } else {
            break;  // timeout
        }
    }
    return 0;  // timeout
}

inline 
bool net::socket_close( int fd, struct net::error_info * err) 
{
    int r = ::close(fd);
    if ( r == 0 ) return true;
    
    if ( err ) err->str = ::strdup(strerror(errno));
    return false;
}

inline 
net::socket_attrib * net::sockattr_from_protocol(net::socket_attrib * attr, const char * name)
{
    attr->proto = 0;
    if ( 0 == strcmp(name, "tcp") || 0 == strcmp(name, "tcp4") ) {
        attr->af   = AF_INET;
        attr->type = SOCK_STREAM;
    } else if ( 0 == strcmp(name, "udp") || 0 == strcmp(name, "udp4") ) {
        attr->af   = AF_INET;
        attr->type = SOCK_DGRAM;
    } else if ( 0 == strcmp(name, "tcp6") ) {
        attr->af   = AF_INET6;
        attr->type = SOCK_STREAM;
    } else if ( 0 == strcmp(name, "udp6") ) {
        attr->af   = AF_INET6;
        attr->type = SOCK_DGRAM;
    } else if ( 0 == strcmp(name, "unix") ) {
        attr->af   = AF_UNIX;
        attr->type = SOCK_STREAM;
    } else {
        return nullptr;
    }
    return attr;
}


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

    const char * remote = argv[1];
    const char * message = argv[2];

    // remote url to net::location
    net::location loc;
    net::location * p = net::location_from_url(&loc, remote);
    if ( !p ) {
        fprintf(stderr, "[error] bad url for pasing, %s\n", remote);
        return -1;
    }

    // open socket channel
    struct net::error_info err;
    int fd = net::socket_open_channel(&loc, &err);
    if ( fd == -1 ) {
        fprintf(stderr, "[error] failed to open socket channel, %s\n", err.str);
        net::free_error_info(&err);
        return -1;
    }

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
    return 0;
}
