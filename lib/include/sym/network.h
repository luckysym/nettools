#pragma once

#include <sym/algorithm.h>
#include <sym/error.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#include <pthread.h>

/// namespace for multi-thread 
namespace mt {
    typedef pthread_mutex_t mutex_t;

    bool mutex_init(mutex_t *m, err::error_t * err);

    bool mutex_free(mutex_t *m, err::error_t * err);

    bool mutex_lock(mutex_t * m, err::error_t * err);

    bool mutex_unlock(mutex_t * m, err::error_t * err);
} // end namespace mt

namespace net {
    
    // socket options bit flags
    const int sockopt_nonblocked  = 1;
    const int sockopt_linger      = 2;
    const int sockopt_reuseaddr   = 4;
    const int sockopt_tcp_nodelay = 8;

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

    const int select_none    = 0;
    const int select_read    = 1;
    const int select_write   = 2;
    const int select_connect = 3;
    const int select_accept  = 4;
    const int select_add     = 8;
    const int select_remove  = 16;
    const int select_timeout = 32;
    const int select_error   = 64;
    const int select_persist = 128;

    const int c_init_list_size = 128;  ///< 列表初始化大小

    const int selopt_thread_safe = 1;  ///< selector support multi-thread

    /// selector event callback function type
    typedef void (*selector_event_calback)(int fd, int event, void *arg);

    /// 事件超时设置节点
    typedef struct select_expiration {
        int       fd;
        int       events;
        int64_t   exp;
        int       inque;
    }  sel_expire_t;
    typedef alg::basic_dlink_node<sel_expire_t> sel_expire_node;

    typedef struct select_item {
        int fd;
        int epevents;    ///< current requested epoll events
        selector_event_calback  callback;
        void   *arg;
        sel_expire_node rdexp;
        sel_expire_node wrexp; 
    } sel_item_t;
    typedef alg::basic_dlink_node<sel_item_t> sel_item_node;

    typedef struct select_operation {
        int     fd;
        int     opevents;  ///< requested operation events
        int64_t expire;
        selector_event_calback  callback;
        void   *arg;
    } sel_oper_t;
    typedef alg::basic_dlink_node<sel_oper_t> sel_oper_node;

    typedef alg::basic_array<epoll_event> epoll_event_array;
    typedef alg::basic_array<sel_item_t>  sel_item_array;
    typedef alg::basic_dlink_list<sel_oper_t> sel_oper_list;
    typedef alg::basic_dlink_list<sel_expire_t> sel_expire_list;

    struct selector_epoll {
        int epfd;     ///< epoll fd
        int count;    ///< total fd registered in epoll
        
        mt::mutex_t      *reqlock;   ///< lock for request operation list(requests) accessing
        sel_oper_list     requests;  ///< request operation queue

        epoll_event_array events; ///< receive the output events from epoll_wait
        sel_item_array    items;    ///< registered items
        
        sel_expire_list   timeouts; ///< timeout queue
        sel_expire_list   persists; ///< persist event list
    };
    typedef struct selector_epoll selector_t;

    /// create and return a selector, returns null if failed
    selector_t * selector_init(selector_t *sel, int options, err::error_t *err);

    /// destroy the selector created by selector_create
    bool selector_destroy(selector_t *sel, err::error_t *err);

    /// add a socket fd and its callback function to the selector.
    bool selector_add(selector_t * sel, int fd, selector_event_calback cb, void *arg, err::error_t *err);

    /// remove socket from selector
    bool selector_remove(selector_t * sel, int fd, err::error_t *err);

    /// request events.
    bool selector_request(selector_t * sel, int fd, int events, int64_t expire, err::error_t *err);

    /// run the selector
    int  selector_run(selector_t *sel, err::error_t *err);

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

    /// 根据协议名称设置socket属性参数
    /// \param attr out, 输出的socket参数。
    /// \param name in, 名称，如tcp, tcp6, udp, udp6, unix
    /// \return 解析成功，返回socket_attrib结构指针，否则返回null
    socket_attrib * sockattr_from_protocol(socket_attrib * attr, const char * name);

    /// 根据locaction设置socketaddr
    socklen_t sockaddr_from_location(sockaddr *paddr, socklen_t len, const location *loc, err::error_t * err);

    /// convert sockaddr to location, returns the localtion pointer if succeed, or null if error 
    location_t * sockaddr_to_location(location_t *loc, const sockaddr * addr, socklen_t len);

    /// send message to remote, and return if all data sent or error or timeout, 
    /// returns total bytes sent if succeeded, or return -1 if failed, or 0 if timeout .
    int socket_sendn(int fd, const char *data, int len, int timeout, err::error_info * err);

    /// receive message from remote, returns total bytes received if data buffer is full, 
    /// or return -1 if failed, or 0 if timeout.
    int socket_recvn(int fd, char *data, int len, int timeout, err::error_info *err);

    /// send message to remote, returns the sent byte count, or -1 if failed
    int socket_send(int fd, const char *data, int len, err::error_info * err);

    /// receive message from remote, returns the recevied byte count, 
    /// or return -1 if any error occured, include remote resets the connection.
    int socket_recv(int fd, char *data, int len, err::error_info *err);

    /// create and open a remote socket channel to the remote location, and returns the socket fd.
    /// returns -1 if failed.
    int socket_open_channel(const location *remote, int options, err::error_info * err);

    /// open a local socket listener, return the socket fd or -1 if failed.
    int socket_open_listener(const location *local, int options, err::error_info *err);

    /// accept a new channel socke from the serve socket fd, return the client socket fd or -1 if nothing accepted.
    /// if -1 returned, check the output error, if err->str is null, means no more channel could be accept,
    /// and if err->str is not null, means accept is error. 
    int socket_accept(int sfd, location_t *remote, err::error_t *err);

    /// close socket fd. returns true if success. 
    bool socket_close(int fd, struct err::error_info *err);
} // end namespace net 

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
int net::socket_open_channel(const net::location * loc, int options, err::error_info * err)
{
    // init socket attributes
    sockattr_t attr;
    sockattr_t * p = sockattr_from_protocol(&attr, loc->proto);
    if ( !p ) {
        if ( err ) err::push_error_info(err, 128, "bad socket protocol name: %s", loc->proto);
        return -1;
    }

    // init socket address
    char addrbuf[128];
    sockaddr *paddr = (sockaddr*)addrbuf;
    err::error_t e2;
    socklen_t addrlen = sockaddr_from_location((sockaddr*)addrbuf, 128, loc, &e2);
    if ( addrlen == 0 ) {
        if ( err ) {
            err::push_error_info(err, 128, "bad socket location addr: %s/%s/%d/%s", 
                loc->proto, loc->host?loc->host:"<none>", loc->port, loc->path?loc->path:"<none>");
        }
        err::free_error_info(&e2);
        return -1;
        
    }

    // create socket
    int nonblock = (options & sockopt_nonblocked)?SOCK_NONBLOCK:0;
    int fd = ::socket(attr.af, attr.type | nonblock | SOCK_CLOEXEC, attr.proto);
    if ( fd == -1 ) {
        if ( err ) {
            err::push_error_info(err, 128, "failed to create socket, %d, %s", errno, strerror(errno));
        }
        return -1;
    }

    // set socket options
    if ( options & sockopt_tcp_nodelay) {
        int opt = 1;
        int r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        if ( r == -1 ) {
            if ( err ) {
                err::push_error_info(
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
                err::push_error_info(err, 128, "setsockopt tcp_nodelay error, fd=%d, err=%d, %s", 
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
            err::push_error_info(err, 128, "connect error, fd=%d, err=%d, %s", 
                fd, errno, strerror(errno));
        }
        close(fd);
        return -1;
    }

    return fd;
} // end net::socket_open_channel

inline 
int net::socket_recvn(int fd, char *data, int len, int timeout, err::error_info * err)
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
                    if ( err ) err::push_error_info(err, 128, "failed to receive data"); 
                    return -1;
                }
                if ( pos == len ) return len;
            } else {
                if ( err ) err::push_error_info(err, 128, "readable event wait failed");
                return -1;
            }
        } else if ( r == 0 ) {
            if ( err ) err::push_error_info(err, 128, "readn timeout");
            break;  // timeout
        } else {
            if ( errno != EINTR ) {
                if ( err ) err::push_error_info(err, 128, "wait failed, %s", strerror(errno));
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
int net::socket_sendn(int fd, const char *data, int len, int timeout, err::error_info * err)
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
                    if ( err ) err::push_error_info(err, 128, "failed to send data, err %d, %s", errno, strerror(errno));
                    return -1;
                }
                if ( pos == len ) return len;
            } else {
                if ( err ) err::push_error_info(err, 128, "writable event wait failed");
                return -1;
            }
        } else if ( r == 0 ) {
            break;  // timeout
        } else {
            if ( errno != EINTR ) {
                if ( err) {
                    err::push_error_info(err, 128, "wait failed, %s", strerror(errno));
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
bool net::socket_close( int fd, struct err::error_info * err) 
{
    int r = ::close(fd);
    if ( r == 0 ) return true;
    
    if ( err ) err::push_error_info(err, 128, "close fd failed, fd:%d, %s", fd, strerror(errno)); 
    return false;
}

inline
net::location_t * net::sockaddr_to_location(net::location_t *loc, const sockaddr* addr, socklen_t len)
{
    location_free(loc);
    if ( addr->sa_family == AF_INET ) {
        char hostbuf[128];
        const sockaddr_in *pin = (const sockaddr_in*)addr;
        const char * host = ::inet_ntop(addr->sa_family, &pin->sin_addr, hostbuf, 128);
        if ( host ) {
            if ( loc->host ) free(loc->host);
            loc->host = strdup(hostbuf);
            loc->port = ntohs(pin->sin_port);
            return loc;
        } else {
            return nullptr;
        }
    } else if ( addr->sa_family == AF_INET6 ) {
        char hostbuf[128];
        const sockaddr_in6 *pin = (const sockaddr_in6*)addr;
        const char * host = ::inet_ntop(addr->sa_family, &pin->sin6_addr, hostbuf, 128);
        if ( host ) {
            if ( loc->host ) free(loc->host);
            loc->host = strdup(hostbuf);
            loc->port = ntohs(pin->sin6_port);
            return loc;
        } else {
            return nullptr;
        }
    } else if ( addr->sa_family == AF_UNIX ) {
        const sockaddr_un *pun = (const sockaddr_un*)addr;
        if ( loc->path ) free(loc->path);
            loc->path = strdup(pun->sun_path);
            return loc;
    } else {
        return nullptr;
    }
}

inline
socklen_t net::sockaddr_from_location(sockaddr *paddr, socklen_t len, const location *loc, err::error_t * err)
{
    // 根据protocol获取af
    sockattr_t attr;
    sockattr_t * p = sockattr_from_protocol(&attr, loc->proto);
    if ( !p ) {
        if ( err ) {
            err::push_error_info(err, 128, "bad location protocol, %s", loc->proto);
        }
        return 0;
    }

    // unix地址
    if ( attr.af == AF_UNIX ) {
        sockaddr_un * unaddr = (sockaddr_un*)paddr;
        if ( loc->path && strlen(loc->path) >= UNIX_PATH_MAX) {
            if ( err ) {
                err::push_error_info(err, 256, "unix path is too long, %s", loc->path);
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
                err::push_error_info(err, 128, "getaddrinfo error, %s, %s", loc->host, gai_strerror(r));
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
int net::socket_open_listener(const net::location *local, int options, err::error_info *err)
{
    // init socket attributes
    sockattr_t attr;
    sockattr_t * p = sockattr_from_protocol(&attr, local->proto);
    if ( !p ) {
        if ( err ) err::push_error_info(err, 128, "bad socket protocol name: %s", local->proto);
        return -1;
    }

    // init socket address
    char addrbuf[128];
    sockaddr *paddr = (sockaddr*)addrbuf;
    err::error_t e2;
    socklen_t addrlen = sockaddr_from_location((sockaddr*)addrbuf, 128, local, &e2);
    if ( addrlen == 0 ) {
        if ( err ) {
            err::push_error_info(err, 128, "bad socket location addr: %s/%s/%d/%s", 
                local->proto, local->host?local->host:"<none>", local->port, local->path?local->path:"<none>");
        }
        err::free_error_info(&e2);
        return -1;
        
    }

    // create socket
    int nonblock = (options & sockopt_nonblocked)?SOCK_NONBLOCK:0;
    int fd = ::socket(attr.af, attr.type | nonblock | SOCK_CLOEXEC, attr.proto);
    if ( fd == -1 ) {
        if ( err ) {
            err::push_error_info(err, 128, "failed to create socket, %d, %s", errno, strerror(errno));
        }
        return -1;
    }

    // set socket options
    if ( options & sockopt_reuseaddr) {
        int opt = 1;
        int r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        if ( r == -1 ) {
            if ( err ) {
                err::push_error_info(
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
            err::push_error_info(err, 128, "bind socket error, fd: %d, %s", fd, strerror(errno));
        }
        close(fd);
        return -1;
    }

    // listen
    r = ::listen(fd, SOMAXCONN);
    if ( r == -1 ) {
        if ( err ) {
            err::push_error_info(err, 128, "bind socket error, fd: %d, %s", fd, strerror(errno));
        }
        close(fd);
        return -1;
    }

    return fd;
} // end socket_open_listener

inline 
int net::socket_send(int fd, const char *data, int len, err::error_t *err)
{
    int r = ::send(fd, data, len, 0);
    if ( r >= 0 ) return r;   // send ok
    else {
        int e = errno;
        if ( e == EINTR || e == EAGAIN) return 0; // nothing send, try again
        else {
            err::push_error_info(err, 128, "socket send error, fd: %d, err: %d, %s",
                fd, e, strerror(e));
            return -1;  // send failed
        }
    }
}

inline 
int net::socket_recv(int fd, char *data, int len, err::error_t *err)
{
    int r = ::recv(fd, data, len, 0);
    if ( r > 0 ) return r;
    if ( r == 0 ) {
        err::push_error_info(err, 128, "socket closed by remote, fd: %d", fd);
        return -1; 
    } else {
        int e = errno;
        if ( e == EINTR || e == EAGAIN) return 0; // nothing recv, try again
        else {
            err::push_error_info(err, 128, "socket recv error, fd: %d, err: %d, %s",
                fd, e, strerror(e));
            return -1;  // recv failed
        }
    }
}

int net::socket_accept(int sfd, location_t *remote, err::error_t *err)
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
            err::free_error_info(err);
            return -1; // nothing recv, try again
        } else {
            err::push_error_info(err, 128, "socket accept error, fd: %d, err: %d, %s",
                sfd, e, strerror(e));
            return -1;  // recv failed
        }
    }
}

inline 
net::selector_t * net::selector_init(net::selector_t *sel, int options, err::error_t *err)
{
    int fd = epoll_create1(EPOLL_CLOEXEC);
    if ( fd == -1 ) {
        err::push_error_info(err, 128, "epoll_create1 error, %d, %s", errno, strerror(errno));
        return nullptr;
    }

    sel->epfd = fd;
    sel->count = 0;
    alg::array_alloc(&sel->items, c_init_list_size);
    alg::array_alloc(&sel->events, c_init_list_size);
    alg::dlinklist_init(&sel->requests);
    alg::dlinklist_init(&sel->timeouts);

    // 根据options设置判断是否需要对request队列创建访问锁
    sel->reqlock = nullptr;
    if ( options & selopt_thread_safe ) {
        sel->reqlock = (mt::mutex_t*)malloc(sizeof(mt::mutex_t));
        assert(sel->reqlock);
        bool isok = mt::mutex_init(sel->reqlock, nullptr);
        assert(isok);
    }

    return sel;
} // end net::selector_init

inline 
bool net::selector_destroy(net::selector_t *sel, err::error_t *err)
{
    if ( sel->count > 0 ) {
        for( int i = 0; i < sel->items.size; ++i ) {
            if ( sel->items.values[i].fd == -1 ) continue;
            int fd = sel->items.values[i].fd;
            auto callback = sel->items.values[i].callback;
            auto arg = sel->items.values[i].arg;
            epoll_ctl(sel->epfd, EPOLL_CTL_DEL, fd, nullptr);
            callback(fd, select_remove, arg);

            sel->items.values[i].fd = -1;
        }
        sel->count = 0;
    }
    alg::array_free(&sel->items);
    
    sel_oper_node * node = dlinklist_pop_front(&sel->requests);
    while ( node ) {
        free(node);
        node = dlinklist_pop_front(&sel->requests);
    }

    sel_expire_node * node1 = dlinklist_pop_front(&sel->timeouts);
    while ( node1 ) {
        free(node1);
        node1 = dlinklist_pop_front(&sel->timeouts);
    }

    alg::array_free(&sel->events);
    close(sel->epfd);
    sel->epfd = -1;

    if ( sel->reqlock ) {
        mt::mutex_free(sel->reqlock, nullptr);
        free( sel->reqlock);
        sel->reqlock = nullptr;
    }
    return true;
} 

inline 
bool net::selector_add(net::selector_t * sel, int fd, selector_event_calback cb, void *arg, err::error_t *err)
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.opevents = select_add;
    node->value.callback = cb;
    node->value.arg = arg;
    node->next = nullptr;
    node->prev = nullptr;

    if ( sel->reqlock ) {
        mt::mutex_lock(sel->reqlock, nullptr);
        auto p = alg::dlinklist_push_back(&sel->requests, node);
        mt::mutex_unlock(sel->reqlock, nullptr);
        assert(p == node);
    } else {
        auto p = alg::dlinklist_push_back(&sel->requests, node);
        assert(p == node);
    }
    
    return true;
}

inline 
bool net::selector_remove(net::selector_t * sel, int fd, err::error_t *err) 
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.opevents = select_remove;
    node->next = nullptr;
    node->prev = nullptr;

    if ( sel->reqlock ) {
        mt::mutex_lock(sel->reqlock, nullptr);
        auto p = alg::dlinklist_push_back(&sel->requests, node);
        mt::mutex_unlock(sel->reqlock, nullptr);
        assert(p == node);
    } else {
        auto p = alg::dlinklist_push_back(&sel->requests, node);
        assert(p == node);
    }

    return true;
}

inline 
bool net::selector_request(net::selector_epoll* sel, int fd, int events, int64_t expire, err::error_info*)
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.opevents = events;
    node->value.expire = expire;
    node->value.callback = nullptr;
    node->value.arg = nullptr;
    node->next = nullptr;
    node->prev = nullptr;
    
    if ( sel->reqlock ) {
        mt::mutex_lock(sel->reqlock, nullptr);
        auto p = alg::dlinklist_push_back(&sel->requests, node);
        mt::mutex_unlock(sel->reqlock, nullptr);
        assert(p == node);
    } else {
        auto p = alg::dlinklist_push_back(&sel->requests, node);
        assert(p == node);
    }
    return true;
}

inline 
int  net::selector_run(net::selector_t *sel, err::error_t *err)
{
    // 从请求队列中取出需要处理的事件操作, 先创建一个新的链表，并将requests链表转移到新建的链表，
    // 转移后清空requests，转移过程对requests加锁，这样等于批量获取，避免长时间对requests加锁。
    sel_oper_list operlst = sel->requests;
    if ( sel->reqlock ) mt::mutex_lock(sel->reqlock, nullptr);
    alg::dlinklist_init(&sel->requests);
    if ( sel->reqlock ) mt::mutex_unlock(sel->reqlock, nullptr);

    // 逐个获取请求节点操作。
    auto rnode = alg::dlinklist_pop_front(&operlst);
    while ( rnode ) {
        assert(rnode->value.fd >= 0);
        if ( rnode->value.opevents == select_add ) {
            // 注册指定的fd
            if ( rnode->value.fd >= sel->items.size ) {
                auto * arr = alg::array_realloc(&sel->items, rnode->value.fd + 1);
                arr->values[rnode->value.fd].fd = -1;
            }
            assert(sel->items.values[rnode->value.fd].fd == -1);
            
            epoll_event evt;
            evt.events = 0;
            evt.data.fd = rnode->value.fd;
            int r1 = epoll_ctl(sel->epfd, EPOLL_CTL_ADD, rnode->value.fd, &evt);
            assert( r1 == 0);
            
            sel_item_t * item = &sel->items.values[rnode->value.fd];
            item->fd = rnode->value.fd;
            item->callback = rnode->value.callback;
            item->arg = rnode->value.arg;
            
            // 读超时节点初始化
            item->rdexp.value.fd = rnode->value.fd;
            item->rdexp.value.events = 0; 
            item->rdexp.value.exp = 0;
            item->rdexp.value.inque = 0;
            item->rdexp.prev = nullptr;
            item->rdexp.next = nullptr;

            // 写超时节点初始化
            item->wrexp.value.fd = rnode->value.fd;
            item->wrexp.value.events = 0; 
            item->wrexp.value.exp = 0;
            item->rdexp.value.inque = 0;
            item->wrexp.prev = nullptr;
            item->wrexp.next = nullptr;

            sel->count += 1;
            alg::array_realloc(&sel->events, sel->count);
        } else if ( rnode->value.opevents == select_remove) {
            // 注销指定的fd
            int r1 = epoll_ctl(sel->epfd, EPOLL_CTL_ADD, rnode->value.fd, nullptr);
            assert( r1 == 0);

            sel_item_t * item = &sel->items.values[rnode->value.fd];
            item->fd = -1;
            
            // 检查timeout队列，删除对应的fd节点
            sel_expire_node * tnode = sel->timeouts.front;
            while ( tnode ) {
                auto tnode2 = tnode->next;
                if (tnode->value.fd == rnode->value.fd ) {
                    alg::dlinklist_remove(&sel->timeouts, tnode);
                }
                tnode = tnode2;
            }
        } else {
            // 设置其他读写事件
            sel_item_t * item = &sel->items.values[rnode->value.fd];

            // 判断是否有新增的需要监听的事件
            int current_events = item->rdexp.value.events | item->wrexp.value.events;
            int events = current_events | rnode->value.opevents;
            int delta_events = events ^ current_events;
            if ( delta_events ) {   // 经过两次位操作，delta_events里面仅剩新增的事件
                struct epoll_event evt;
                evt.events  = 0;
                evt.data.fd = item->fd;
                if ( events & select_read || events & select_accept ) evt.events |= EPOLLIN;
                if ( events & select_write || events & select_connect ) evt.events |= EPOLLOUT;
                
                // 修改epoll
                int r = epoll_ctl(sel->epfd, EPOLL_CTL_MOD, item->fd, &evt);
                assert( r == 0);
                item->epevents = evt.events;
                
                if ( 0 == (delta_events & select_persist) ) {
                    // 非持久事件
                    if ( delta_events & select_read || delta_events & select_accept ) {
                        item->rdexp.value.events |= ( delta_events & (select_read |select_accept) ) ;
                        item->rdexp.value.exp = rnode->value.expire;
                        assert( item->rdexp.value.inque == 0 );
                        alg::dlinklist_push_back(&sel->timeouts, &item->rdexp);
                        item->rdexp.value.inque = 1;   // 在timeout queue
                    }
                    if ( delta_events & select_write || delta_events & select_connect ) {
                        item->wrexp.value.events |= ( delta_events & (select_write |select_connect) ) ;
                        item->wrexp.value.exp = rnode->value.expire;
                        assert( item->wrexp.value.inque == 0 );
                        alg::dlinklist_push_back(&sel->timeouts, &item->wrexp);
                        item->wrexp.value.inque = 1;   // 在timeout queue
                    }
                } else {
                    // 持久事件
                    if ( delta_events & select_read || delta_events & select_accept ) {
                        item->rdexp.value.exp = rnode->value.expire;
                        item->rdexp.value.events = delta_events & (select_read | select_accept | select_persist);
                        assert( item->rdexp.value.inque == 0 );
                        alg::dlinklist_push_back(&sel->persists, &item->rdexp);
                        item->rdexp.value.inque = 2;   // 在persist queue
                    }
                    if ( delta_events & select_write || delta_events & select_connect ) {
                        item->wrexp.value.exp = rnode->value.expire;
                        item->wrexp.value.events = delta_events & (select_write | select_connect | select_persist);
                        assert( item->wrexp.value.inque == 0 );
                        alg::dlinklist_push_back(&sel->persists, &item->wrexp);
                        item->wrexp.value.inque = 2;   // 在persist queue
                    }
                }
            }
        }
        free(rnode);
    }

    // 获取超时事件
    auto node = sel->timeouts.front;
    if ( !node ) node = sel->persists.front;
    if ( !node ) return 0;  // no any event need wait
    
    int64_t now = net::now();
    int timeout = (int)((node->value.exp - now) / 1000);
    if ( timeout < 0 )  timeout = 0;  // check event without timeout if first node already timeout

    // epoll wait
    int r = epoll_wait(sel->epfd, sel->events.values, sel->events.size, timeout);
    if ( r > 0 ) {
        for(int i = 0; i < r; ++i ) {
            epoll_event * e = sel->events.values + i;
            int fd = e->data.fd;
            assert( fd < sel->items.size );
            sel_item_t *item = sel->items.values + fd;
            
            if ( e->events & EPOLLIN ) {
                if ( item->rdexp.value.events & select_read ) {
                    item->callback(fd, select_read, item->arg);
                } else if ( item->rdexp.value.events & select_accept ) {
                    item->callback(fd, select_accept, item->arg);
                } else {
                    // 没有请求EPOLLIN事件，怎么会有呢？
                    assert( "EPOLLIN event not requested" == nullptr );
                }
                // 非持久事件，取消该标记
                if ( 0 == (item->rdexp.value.events & select_persist) ) {
                    item->rdexp.value.events &= ~(select_read|select_accept);
                }
            }
            if ( e->events & EPOLLOUT ) {
                if ( item->wrexp.value.events & select_write ) {
                    item->callback(fd, select_write, item->arg);
                } else if ( item->wrexp.value.events & select_connect ) {
                    item->callback(fd, select_connect, item->arg);
                } else {
                    // 没有请求EPOLLIN事件，怎么会有呢？
                    assert( "EPOLLOUT event not requested" == nullptr );
                }
                // 非持久事件，取消该标记
                if ( 0 == (item->wrexp.value.events & select_persist) ) {
                    item->wrexp.value.events &= ~(select_write|select_connect);
                }
            }
            // wait错误
            if ( e->events & EPOLLERR ) {
                item->callback(fd, select_error, item->arg);
            }

            // 遍历timeout列表，剔除当前相关事件，这里遍历性能差点，后续优化
            auto tnode = sel->timeouts.front;
            while ( tnode ) {
                auto tnode2 = tnode->next;
                assert(tnode->value.inque == 1);
                if ( tnode->value.fd == e->data.fd ) {
                    int se = 0;
                    if ( e->events & EPOLLIN ) se = select_read | select_accept;
                    if ( e->events & EPOLLOUT) se = select_write | select_connect;
                    if ( tnode->value.events & se ) {
                        tnode->value.events &= ( ~se ); // 取消请求的epoll事件，如果所有事件都取消了，就删除该节点
                        alg::dlinklist_remove(&sel->timeouts, tnode);
                        break;
                    }
                }
                tnode = tnode2;
            }
        } // end for
    } else if ( r == -1 ) {
        // 出错了
        push_error_info(err, 128, "epoll wait error, epfd: %d, err: %d, %s", sel->epfd, errno, strerror(errno));
    }

    now = net::now();
    // 清理timeout列表超时节点，这里遍历性能差点，后续优化
    auto tnode = sel->timeouts.front;
    while ( tnode ) {
        auto tnode2 = tnode->next;
        if ( tnode->value.exp < now ) {
            int fd = tnode->value.fd;
            assert( fd < sel->items.size );
            sel_item_t *item = sel->items.values + fd;
            item->callback(fd, select_timeout, item->arg);
            
            // timeouts队列里必然都是非持久事件，从timeout表里清除
            assert(tnode->value.inque == 1);
            tnode->value.events = select_none;
            alg::dlinklist_remove(&sel->timeouts, tnode);
            tnode->next = nullptr;
            tnode->prev = nullptr;
            tnode->value.inque = 0;
        }
        tnode = tnode2;
    }

    return r;
} // net::selector_run

