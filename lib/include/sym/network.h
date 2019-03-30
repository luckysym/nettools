#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

namespace net {
    
    // socket options bit flags
    const int sockopt_nonblocked  = 1;
    const int sockopt_linger      = 2;
    const int sockopt_reuseaddr   = 4;
    const int sockopt_tcp_nodelay = 8;
    
    typedef struct error_info {
        char * str;
        struct error_info * next;
    } error_t;

    typedef struct socket_attrib {
        int af;
        int type;
        int proto;
    } sockattr_t;

    typedef struct location {
        char * proto;
        char * host;
        int    port;
        char * path;
    } location_t;

    typedef struct buffer {
        char *    data;
        int       size;
        int       begin;
        int       end;    
    } buffer_t;

    struct selector_epoll {

    };
    typedef struct selector_epoll selector_t;

    const int select_read    = 1;
    const int select_write   = 2;
    const int select_connect = 3;
    const int select_accept  = 4;
    const int select_add     = 8;
    const int select_remove  = 16;
    const int select_error   = 32;

    /// extend the buffer capacity to the new size
    buffer_t * buffer_alloc(buffer_t *buf, int size);

    /// free the buffer members, release its memory
    void buffer_free(buffer_t *buf);

    /// realloc the buffer with the new capacity size.
    buffer_t * buffer_realloc(buffer_t *buf, int size);

    /// move the data in the buffer to the front
    buffer_t * buffer_pullup(buffer_t *buf);

    /// selector event callback function type
    typedef void (*selector_event_calback)(selector_t *, int fd, int event, void *arg);

    /// create and return a selector, returns null if failed
    selector_t * selector_create(int options, error_t *err);

    /// destroy the selector created by selector_create
    bool selector_destroy(selector_t *sel, error_t *err);

    /// add a socket fd and its callback function to the selector.
    bool selector_add(selector_t * sel, int fd, selector_event_calback cb, void *arg, error_t *err);

    /// remove socket from selector
    bool selector_remove(selector_t * sel, int fd, error_t *err);

    /// request events.
    bool selector_request(selector_t * sel, int fd, int events, int timeout, error_t *err);

    /// run the selector
    int  selector_run(selector_t *sel, error_t *err);

    /// gets and returns current timestamp in microseconds from epoch-time 
    int64_t now();

    /// init a location struct
    struct location * location_init(struct location *loc);

    /// free the location string members
    void location_free(struct location *loc);    

    /// copy location from src to dest, the dest should be free by location_free
    void location_copy(location_t * dest, const location_t *src);
    
    /// parse url to location, return the localtion pointer if success, null if failed.
    location_t * location_from_url(struct location * loc, const char * url);

    /// free location members
    void location_free(struct localtion * loc);

    /// init the error info
    void init_error_info(error_t * err);

    /// free error info members
    void free_error_info(error_t * err);

    /// push error info to the error object
    void push_error_info(error_t * err, int size, const char * format, ...);

    /// 根据协议名称设置socket属性参数
    /// \param attr out, 输出的socket参数。
    /// \param name in, 名称，如tcp, tcp6, udp, udp6, unix
    /// \return 解析成功，返回socket_attrib结构指针，否则返回null
    socket_attrib * sockattr_from_protocol(socket_attrib * attr, const char * name);

    /// 根据locaction设置socketaddr
    socklen_t sockaddr_from_location(sockaddr *paddr, socklen_t len, const location *loc, error_t * err);

    /// convert sockaddr to location, returns the localtion pointer if succeed, or null if error 
    location_t * sockaddr_to_location(location_t *loc, const sockaddr * addr, socklen_t len);

    /// send message to remote, and return if all data sent or error or timeout, 
    /// returns total bytes sent if succeeded, or return -1 if failed, or 0 if timeout .
    int socket_sendn(int fd, const char *data, int len, int timeout, error_info * err);

    /// receive message from remote, returns total bytes received if data buffer is full, 
    /// or return -1 if failed, or 0 if timeout.
    int socket_recvn(int fd, char *data, int len, int timeout, error_info *err);

    /// send message to remote, returns the sent byte count, or -1 if failed
    int socket_send(int fd, const char *data, int len, error_info * err);

    /// receive message from remote, returns the recevied byte count, 
    /// or return -1 if any error occured, include remote resets the connection.
    int socket_recv(int fd, char *data, int len, error_info *err);

    /// create and open a remote socket channel to the remote location, and returns the socket fd.
    /// returns -1 if failed.
    int socket_open_channel(const location *remote, int options, error_info * err);

    /// open a local socket listener, return the socket fd or -1 if failed.
    int socket_open_listener(const location *local, int options, error_info *err);

    /// accept a new channel socke from the serve socket fd, return the client socket fd or -1 if nothing accepted.
    /// if -1 returned, check the output error, if err->str is null, means no more channel could be accept,
    /// and if err->str is not null, means accept is error. 
    int socket_accept(int sfd, location_t *remote, error_t *err);

    /// close socket fd. returns true if success. 
    bool socket_close(int fd, struct error_info *err);
} // end namespace net 

inline 
void net::buffer_free(net::buffer_t *buf) 
{
    if ( buf->data ) free(buf->data);
    buf->data = nullptr;
    buf->size = 0;
    buf->begin = 0;
    buf->end = 0;
}

inline 
net::buffer_t * net::buffer_alloc(net::buffer_t *buf, int size)
{
    if ( size > 0 ) {
        buf->data = (char *)malloc(size);
        assert(buf->data);
    } else {
        buf->data = nullptr;
    }
    buf->size  = 0;
    buf->begin = 0;
    buf->end   = 0;
    return buf;
}

inline 
net::buffer_t * net::buffer_realloc(net::buffer_t *buf, int size)
{
    if ( size == 0 ) {
        net::buffer_free(buf);
        return buf;
    }
    void * p = realloc(buf->data, size);
    buf->data = (char*)p;
    buf->size = size;
    if ( buf->begin > buf->size ) buf->begin = buf->size;
    if ( buf->end > buf->size ) buf->end = buf->size;
    return buf;
}

inline 
net::buffer_t * net::buffer_pullup(net::buffer_t *buf) 
{
    if ( buf->begin = 0 ) return buf;

    int len = buf->end - buf->begin;
    void * p = memmove(buf->data, buf->data + buf->begin, len);
    assert(p == buf->data);
    buf->begin -= len;
    buf->end -= len;
    return buf;
}

inline
int64_t net::now() 
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

inline 
net::location * net::location_init(net::location *loc)
{
    loc->proto = nullptr;
    loc->host = nullptr;
    loc->port = 0;
    loc->path = nullptr;
    return loc;
}

inline 
void net::location_free(net::location * loc) {
    if ( loc->proto ) free((void *)loc->proto);
    if ( loc->host )  free((void *)loc->host);
    if ( loc->path )  free((void *)loc->path);

    loc->proto = nullptr;
    loc->host = nullptr;
    loc->port = 0;
    loc->path = nullptr;
}

inline 
void net::location_copy(net::location_t * dest, const net::location_t *src)
{
    net::location_free(dest);
    if ( src->proto ) dest->proto = strdup(src->proto);
    if ( src->host  ) dest->host  = strdup(src->host );
    if ( src->path  ) dest->path  = strdup(src->path );
    dest->port = src->port; 
}

inline 
net::location * net::location_from_url(struct net::location * loc, const char * url)
{
        // 解析protocol
        const char * proto = url;
        char * p0 = ::strstr((char*)proto, "://");
        if ( !p0 ) return nullptr;
        int proto_len = p0 - proto;
        
        // 解析host
        const char * host = p0 + 3;
        int host_len;
        while ( *host == ' ' ) ++host;
        if ( *host == '[')  {
            host += 1;
            p0 = ::strchr((char*)host, ']');
            if ( !p0 ) return nullptr;
        } else {
            p0 = ::strchr((char*)host, ':');
            if ( !p0 ) p0 = ::strchr((char*)host, '/');
        }
        if ( p0 ) host_len = p0 - host;
        else host_len = ::strlen(host);

        // 解析port
        const char * port = (*p0 == ':')?(p0 + 1):nullptr;

        // 解析path
        const char * path = (*p0 == '/')?p0:nullptr;

        location_free(loc);
        if ( proto ) {
            loc->proto = (char *)malloc(proto_len + 1);
            memcpy(loc->proto, proto, proto_len);
            loc->proto[proto_len] = '\0';
        }
        if ( host ) {
            loc->host = (char*)malloc(host_len + 1);
            memcpy(loc->host, host, host_len);
            loc->host[host_len] = '\0';
        } 
        if ( port ) loc->port = atoi(port);
        if ( path ) loc->path = strdup(path);
        return loc;
}

inline 
void net::init_error_info(net::error_info * err)
{
    err->str = nullptr;
    err->next = nullptr;
}

inline 
void net::free_error_info(net::error_info * err)
{
    error_t * p = err->next;
    while ( p ) {
        error_t * p1 = p->next;
        if ( p->str ) {
            free(p->str);
            p->str = nullptr;
        }
        p->next = nullptr;
        free(p);
        p = p1;
    }

    if ( err->str ) {
        free((void *)err->str);
        err->str = nullptr;
    }
    err->next = nullptr;
}

inline 
void net::push_error_info(net::error_info * err, int size, const char *format, ...) {
    if( err->str ) {
        err->next = (error_t*)malloc(sizeof(error_t));
        err->next->str = err->str;
    }

    err->str = (char*)malloc(size);
    va_list ap;
    va_start(ap, format);
    vsnprintf(err->str, size, format, ap);
    va_end(ap);
}

inline 
int net::socket_open_channel(const net::location * loc, int options, net::error_info * err)
{
    // init socket attributes
    sockattr_t attr;
    sockattr_t * p = sockattr_from_protocol(&attr, loc->proto);
    if ( !p ) {
        if ( err ) net::push_error_info(err, 128, "bad socket protocol name: %s", loc->proto);
        return -1;
    }

    // init socket address
    char addrbuf[128];
    sockaddr *paddr = (sockaddr*)addrbuf;
    error_t e2;
    socklen_t addrlen = sockaddr_from_location((sockaddr*)addrbuf, 128, loc, &e2);
    if ( addrlen == 0 ) {
        if ( err ) {
            net::push_error_info(err, 128, "bad socket location addr: %s/%s/%d/%s", 
                loc->proto, loc->host?loc->host:"<none>", loc->port, loc->path?loc->path:"<none>");
        }
        free_error_info(&e2);
        return -1;
        
    }

    // create socket
    int nonblock = (options & sockopt_nonblocked)?SOCK_NONBLOCK:0;
    int fd = ::socket(attr.af, attr.type | nonblock | SOCK_CLOEXEC, attr.proto);
    if ( fd == -1 ) {
        if ( err ) {
            net::push_error_info(err, 128, "failed to create socket, %d, %s", errno, strerror(errno));
        }
        return -1;
    }

    // set socket options
    if ( options & sockopt_tcp_nodelay) {
        int opt = 1;
        int r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        if ( r == -1 ) {
            if ( err ) {
                net::push_error_info(
                    err, 128, "setsockopt tcp_nodelay error, fd=%d, err=%d, %s", 
                    fd, errno, strerror(errno));
            }
            close(fd);
            return -1;
        }
    }
    if ( options & sockopt_linger) {
        struct linger lg;
        lg.l_onoff  = 1;
        lg.l_linger = 30;
        int r = setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
        if ( r == -1 ) {
            if ( err ) {
                net::push_error_info(err, 128, "setsockopt tcp_nodelay error, fd=%d, err=%d, %s", 
                    fd, errno, strerror(errno));
            }
            close(fd);
            return -1;
        }
    } 

    // connect to remote
    int r = connect(fd, paddr, addrlen);
    if ( r == -1 && errno != EINPROGRESS ) {
        if ( err ) {
            net::push_error_info(err, 128, "connect error, fd=%d, err=%d, %s", 
                fd, errno, strerror(errno));
        }
        close(fd);
        return -1;
    }

    return fd;
} // end net::socket_open_channel

inline 
int net::socket_recvn(int fd, char *data, int len, int timeout, net::error_info * err)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    int64_t now = net::now();
    int64_t exp = now + (int64_t)timeout * 1000;
    int pos = 0;

    while ( timeout != 0 ) {
        int r = poll(&pfd, 1, timeout);
        if ( r > 0 ) {
            if ( pfd.revents & POLLIN) {
                ssize_t n = ::recv(fd, data + pos, len - pos, 0);
                if ( n >= 0) {
                    pos += n;
                } else {
                    if ( err ) net::push_error_info(err, 128, "failed to receive data"); 
                    return -1;
                }
                if ( pos == len ) return len;
            } else {
                if ( err ) net::push_error_info(err, 128, "readable event wait failed");
                return -1;
            }
        } else if ( r == 0 ) {
            if ( err ) net::push_error_info(err, 128, "readn timeout");
            break;  // timeout
        } else {
            if ( errno != EINTR ) {
                if ( err ) net::push_error_info(err, 128, "wait failed, %s", strerror(errno));
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
int net::socket_sendn(int fd, const char *data, int len, int timeout, net::error_info * err)
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
            if ( pfd.revents & POLLOUT) {
                ssize_t n = ::send(fd, data + pos, len - pos, 0);
                if ( n >= 0) {
                    pos += n;
                } else {
                    if ( err ) net::push_error_info(err, 128, "failed to send data, err %d, %s", errno, strerror(errno));
                    return -1;
                }
                if ( pos == len ) return len;
            } else {
                if ( err ) net::push_error_info(err, 128, "writable event wait failed");
                return -1;
            }
        } else if ( r == 0 ) {
            break;  // timeout
        } else {
            if ( errno != EINTR ) {
                if ( err) {
                    net::push_error_info(err, 128, "wait failed, %s", strerror(errno));
                }
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
    
    if ( err ) net::push_error_info(err, 128, "close fd failed, fd:%d, %s", fd, strerror(errno)); 
    return false;
}

inline
socklen_t net::sockaddr_from_location(sockaddr *paddr, socklen_t len, const location *loc, error_t * err)
{
    // 根据protocol获取af
    sockattr_t attr;
    sockattr_t * p = sockattr_from_protocol(&attr, loc->proto);
    if ( !p ) {
        if ( err ) {
            net::push_error_info(err, 128, "bad location protocol, %s", loc->proto);
        }
        return 0;
    }

    // unix地址
    if ( attr.af == AF_UNIX ) {
        sockaddr_un * unaddr = (sockaddr_un*)paddr;
        if ( loc->path && strlen(loc->path) >= UNIX_PATH_MAX) {
            if ( err ) {
                net::push_error_info(err, 256, "unix path is too long, %s", loc->path);
            }
            return 0;
        }
        strncpy(unaddr->sun_path, loc->path, UNIX_PATH_MAX);
        return sizeof(sockaddr_un);
    }

    // 查找IP主机地址
    struct addrinfo *res = nullptr;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = attr.af;

    if ( strcmp(loc->host, "*") ) {
        int r = ::getaddrinfo(loc->host, nullptr, &hints, &res);
        if ( r != 0 ) {
            if ( err ) {
                net::push_error_info(err, 128, "getaddrinfo error, %s, %s", loc->host, gai_strerror(r));
            }
            return 0;
        }
        memcpy(paddr, res->ai_addr, res->ai_addrlen);     // 地址整体复制输出
    } else {
        // 任意地址，除了sa_family和port，其余都是0
        memset(paddr, 0, len);  
        paddr->sa_family = attr.af;
    }
    
    ((sockaddr_in*)paddr)->sin_port = htons(loc->port);   // 设置端口，in和in6端口字段位置相同
    return res->ai_addrlen;
} // end net::sockaddr_from_location

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

inline
int net::socket_open_listener(const net::location *local, int options, net::error_info *err)
{
    // init socket attributes
    sockattr_t attr;
    sockattr_t * p = sockattr_from_protocol(&attr, local->proto);
    if ( !p ) {
        if ( err ) net::push_error_info(err, 128, "bad socket protocol name: %s", local->proto);
        return -1;
    }

    // init socket address
    char addrbuf[128];
    sockaddr *paddr = (sockaddr*)addrbuf;
    error_t e2;
    socklen_t addrlen = sockaddr_from_location((sockaddr*)addrbuf, 128, local, &e2);
    if ( addrlen == 0 ) {
        if ( err ) {
            net::push_error_info(err, 128, "bad socket location addr: %s/%s/%d/%s", 
                local->proto, local->host?local->host:"<none>", local->port, local->path?local->path:"<none>");
        }
        free_error_info(&e2);
        return -1;
        
    }

    // create socket
    int nonblock = (options & sockopt_nonblocked)?SOCK_NONBLOCK:0;
    int fd = ::socket(attr.af, attr.type | nonblock | SOCK_CLOEXEC, attr.proto);
    if ( fd == -1 ) {
        if ( err ) {
            net::push_error_info(err, 128, "failed to create socket, %d, %s", errno, strerror(errno));
        }
        return -1;
    }

    // set socket options
    if ( options & sockopt_reuseaddr) {
        int opt = 1;
        int r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        if ( r == -1 ) {
            if ( err ) {
                net::push_error_info(
                    err, 128, "setsockopt tcp_nodelay error, fd=%d, err=%d, %s", 
                    fd, errno, strerror(errno));
            }
            close(fd);
            return -1;
        }
    }

    // bind socket to address
    int r = ::bind(fd, paddr, addrlen);
    if ( r == -1 ) {
        if ( err ) {
            net::push_error_info(err, 128, "bind socket error, fd: %d, %s", fd, strerror(errno));
        }
        close(fd);
        return -1;
    }

    // listen
    r = ::listen(fd, SOMAXCONN);
    if ( r == -1 ) {
        if ( err ) {
            net::push_error_info(err, 128, "bind socket error, fd: %d, %s", fd, strerror(errno));
        }
        close(fd);
        return -1;
    }

    return fd;
} // end socket_open_listener

inline 
int net::socket_send(int fd, const char *data, int len, net::error_t *err)
{
    int r = ::send(fd, data, len, 0);
    if ( r >= 0 ) return r;   // send ok
    else {
        int e = errno;
        if ( e == EINTR || e == EAGAIN) return 0; // nothing send, try again
        else {
            net::push_error_info(err, 128, "socket send error, fd: %d, err: %d, %s",
                fd, e, strerror(e));
            return -1;  // send failed
        }
    }
}

inline 
int net::socket_recv(int fd, char *data, int len, net::error_t *err)
{
    int r = ::recv(fd, data, len, 0);
    if ( r > 0 ) return r;
    if ( r == 0 ) {
        net::push_error_info(err, 128, "socket closed by remote, fd: %d", fd);
        return -1; 
    } else {
        int e = errno;
        if ( e == EINTR || e == EAGAIN) return 0; // nothing recv, try again
        else {
            net::push_error_info(err, 128, "socket recv error, fd: %d, err: %d, %s",
                fd, e, strerror(e));
            return -1;  // recv failed
        }
    }
}

int net::socket_accept(int sfd, location_t *remote, error_t *err)
{
    char addrbuf[128];
    socklen_t addrlen = 128;
    int cfd = ::accept(sfd, (sockaddr *)addrbuf, &addrlen);
    if ( cfd >= 0 ) {
        // new connection accepted
        location_t * p = net::sockaddr_to_location(remote, (sockaddr*)addrbuf, addrlen);
        assert(p);
        return cfd;
    } else {
        int e = errno;
        if ( e == EINTR || e == EAGAIN) {
            net::free_error_info(err);
            return -1; // nothing recv, try again
        } else {
            net::push_error_info(err, 128, "socket accept error, fd: %d, err: %d, %s",
                sfd, e, strerror(e));
            return -1;  // recv failed
        }
    }
}