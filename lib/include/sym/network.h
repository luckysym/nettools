#pragma once

#include <sym/algorithm.h>
#include <sym/error.h>
#include <sym/chrono.h>

#include <sys/types.h>
#include <sys/socket.h>
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


#define SYM_SOCK_RV_RETURN(rv) \
    do { \
        if ( rv == 0 ) return true; \
        if (e) *e = err::Error(errno, err::dmSystem); \
    } while (0)

/// 包含网络及socket相关操作的名称空间。
namespace net {
    
    // socket options bit flags
    const int sockopt_nonblocked  = 1;
    const int sockopt_linger      = 2;
    const int sockopt_reuseaddr   = 4;
    const int sockopt_tcp_nodelay = 8;

    const int sock_shut_read = SHUT_RD;
    const int sock_shut_write = SHUT_WR;
    const int sock_shut_both  = SHUT_RDWR;

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

    enum {
        shutdownRead  = SHUT_RD,
        shutdownWrite = SHUT_WR,
        shutdownBoth  = shutdownRead + shutdownWrite
    };

    class Address {
    public:
        static const int ADDR_BUFFER_LEN  = 128;
    private:
        char m_addrbuf[ADDR_BUFFER_LEN];
        socklen_t m_size { sizeof(struct sockaddr) };
    public:
        Address() { ((sockaddr *)m_addrbuf)->sa_family = AF_UNSPEC; }
        Address(const char * host, int port, err::Error *e );
        
        int af() const { return data()->sa_family; }
        socklen_t capacity() const { return ADDR_BUFFER_LEN; }
        sockaddr * data() { return (sockaddr *)m_addrbuf; }
        const sockaddr * data() const {return (const sockaddr *)m_addrbuf;} 
        void resize(int addrlen) { assert(addrlen <= ADDR_BUFFER_LEN); m_size = addrlen; }
        socklen_t size() const { return m_size; }

    public:
        static socklen_t HostNameToAddress(const char *host, sockaddr * addr, socklen_t len, err::Error *e);
    }; // end class Address

    class Socket{
    private:
        int m_fd    {-1};

    public:
        Socket() {}
        Socket(int fd) : m_fd(fd) {}
        int  accept(Address * remote, int flags, err::Error * e = nullptr);
        bool bind(const Address & addr, err::Error * e = nullptr);
        bool close(err::Error *e = nullptr);
        bool create(int af, int type, err::Error *e = nullptr);
        int  fd() const  { return m_fd; }
        bool listen(err::Error * e = nullptr);
        int  receive(char * buf, int len, err::Error * e = nullptr);
        int  send(const char * buf, int len, err::Error *e = nullptr);
        bool shutdown(int how, err::Error *e = nullptr);
    }; // end class Socket

    class SocketOption 
    {
    public:
        SocketOption(
            Socket & sock, 
            int level, 
            int optname, 
            void * optval, 
            socklen_t optlen, 
            err::Error * e = nullptr);
    };

    class SocketOptReuseAddr : public SocketOption 
    {
    public:
        SocketOptReuseAddr(Socket & sock, int value, err::Error * e = nullptr)
            : SocketOption(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value), e) 
        {}
    }; // end class SocketOptReuseAddr

    class SocketOptKeepAlive : public SocketOption 
    {
    public:
        SocketOptKeepAlive(Socket & sock, int value, err::Error * e = nullptr)
            : SocketOption(sock, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value), e)
        {}
    };

} // end namespace net 

namespace net
{
    inline 
    int Socket::accept(Address * remote, int flags, err::Error * e)
    {
        sockaddr * paddr = remote->data();
        socklen_t  addrlen = remote->capacity();

        int rv = accept4(m_fd, paddr, &addrlen, flags);
        if ( rv >= 0 ) {
            remote->resize(addrlen);
            return rv;
        }

        // error handler, 对于中断或者非阻塞模式下没有新连接，虽然返回-1，但输出的Error中错误值是0.
        int eno = errno;
        if ( eno == EAGAIN || eno == EINTR ) {
            if ( e ) e->clear();
        } else {
            if (e) *e = err::Error(eno, err::dmSystem);
        }
        return -1;
    }

    inline 
    bool Socket::bind(const Address & addr, err::Error * e)
    {
        int rv = ::bind(m_fd, addr.data(), addr.size());
        SYM_SOCK_RV_RETURN(rv);
    }

    inline
    bool Socket::close(err::Error * e)
    {
        int rv = ::close(m_fd);
        SYM_SOCK_RV_RETURN(rv);
    }

    inline 
    bool Socket::create(int af, int type, err::Error *e )
    {
        assert( m_fd == -1);
        int rv = ::socket(af, type, 0);
        if ( rv >= 0 ) {
            m_fd = rv;
            return true;
        }

        // error handler
        if (e)  *e = err::Error(errno, err::dmSystem);
        return false;
    }

    inline 
    bool Socket::listen(err::Error * e)
    {
        int rv = ::listen(m_fd, SOMAXCONN);
        SYM_SOCK_RV_RETURN(rv);
    }

    inline 
    bool Socket::shutdown(int how, err::Error * e)
    {
        SYM_TRACE_VA("[trace] SOCKET_SHUTDOWN, fd: %d, how: %d", m_fd, how);
        int rv = ::shutdown(m_fd, how);
        SYM_SOCK_RV_RETURN(rv);
    }

    inline 
    int Socket::receive(char * buf, int len, err::Error * e)
    {
        ssize_t rv = ::recv(m_fd, buf, len, 0);
        if ( rv > 0 )  {
            return (int)rv;
        } else if ( rv == 0 ) {
            if ( e ) *e = err::Error(-1, "connection is reset by peer");
            return -1;
        } else {
            int eno = errno;
            if ( eno == EAGAIN || eno == EINTR )  {
                return 0;
            } else {
                if ( e ) *e = err::Error(errno, err::dmSystem);
                return -1;
            }
        }
    }

    inline 
    int Socket::send(const char * buf, int len, err::Error *e)
    {
        ssize_t rv = ::send(m_fd, buf, len, 0);
        if ( rv >= 0 )  return (int)rv;
        else {
            int eno = errno;
            if ( eno == EAGAIN || eno == EINTR ) return 0;
            else {
                if ( e ) *e = err::Error(errno, err::dmSystem);
                return -1;
            }
        }
    }

} // end namespace net

namespace net
{

    inline 
    Address::Address(const char * host, int port, err::Error *e)
    {
        socklen_t len = HostNameToAddress(host, (sockaddr *)m_addrbuf, ADDR_BUFFER_LEN, e);
        if ( len == 0 ) return ;  // error

        m_size = len;
        if ( data()->sa_family == AF_INET ) {
            sockaddr_in * in = (sockaddr_in*)m_addrbuf;
            in->sin_port = htons(port);
        } else if ( data()->sa_family == AF_INET6 ) {
            sockaddr_in6 * in6 = (sockaddr_in6*)m_addrbuf;
            in6->sin6_port = htons(port);
        } else {
            if ( e ) *e = err::Error(-1, "address of the host is not an ipv4/ipv6 address");
        }
        return ;
    }

    inline 
    socklen_t Address::HostNameToAddress(const char *host, sockaddr * addr, socklen_t len, err::Error *e)
    {
        assert(host);
        struct addrinfo *res = nullptr;
	    struct addrinfo hints;
	    hints.ai_flags = AI_PASSIVE;
	    hints.ai_family = AF_UNSPEC;
	    hints.ai_socktype = 0;
	    hints.ai_protocol = 0;
	    hints.ai_addrlen = 0;
	    hints.ai_addr = nullptr;
	    hints.ai_canonname = nullptr;
	    hints.ai_next = nullptr;
        
        socklen_t retlen = 0;
        int rv = ::getaddrinfo(host, nullptr, &hints, &res);
        if ( rv == 0 ) {
            struct addrinfo * p = res;
            for( ; p != nullptr; p = p->ai_next) {
                if ( p->ai_addr == nullptr || len < p->ai_addrlen) continue;
                memcpy(addr, p->ai_addr, p->ai_addrlen);
                retlen = p->ai_addrlen;
                break;
            }
        } else {
            if ( e ) *e = err::Error(rv, gai_strerror(rv));
        }
        if ( res ) freeaddrinfo(res);
        return retlen;
    }

    inline 
    SocketOption::SocketOption(
        Socket & sock, 
        int level, 
        int optname, 
        void * optval, 
        socklen_t optlen, 
        err::Error * e)
    {
        int r = ::setsockopt(sock.fd(), level, optname, optval, optlen);
        if ( r == -1 ) {
            if ( e ) *e = err::Error(errno, err::dmSystem);
        }
        return ;
    }

} // end namespace net