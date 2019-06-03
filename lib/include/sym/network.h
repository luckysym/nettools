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
    
    /// \brief URL对象， [schema://][user[:passwd]@][host[:port]][/path][?query][#label]
    class URL {
    public:
        std::string m_schema;
        std::string m_user;
        std::string m_password;
        std::string m_host;
        std::string m_port;
        std::string m_path;
        std::string m_query;

        void clear() {
            m_schema.clear();
            m_user.clear();
            m_password.clear();
            m_host.clear();
            m_port.clear();
            m_path.clear();
            m_query.clear();
        }

        std::string  toString() const;
    }; // end class URL

    /// \brief URL解析器
    class URLParser
    {
    public:
        bool parse(URL * url, const char * str, err::Error * e = nullptr);
    protected:
        const char * parseSchema(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parseUserPass(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parseHostPort(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parsePath(URL * url, const char * str, int len, err::Error * e = nullptr);
        const char * parseQuery(URL * url, const char * str, int len, err::Error * e = nullptr);
    private:
        const char * find(const char * str, int len1, const char chars[], int len2);
        const char * rfind(const char * str, int len1, const char chars[], int len2);
    }; // end class URLParser

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
        bool connect(const Address & remote, err::Error * e = nullptr);
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
    bool Socket::connect(const Address & remote, err::Error * e)
    {
        while (1) {
            int r = ::connect(m_fd, remote.data(), remote.size());
            if ( r == 0 ) return true;
            
            // r == -1
            int eno = errno;
            if ( eno == EINTR ) continue;
            else if ( eno == EINPROGRESS ) return true;
            else {
                if ( e ) *e = err::Error(eno, err::dmSystem);
                return false;
            }
        }
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

namespace net
{
    bool URLParser::parse(URL * url, const char * str, err::Error * e)
    {
        int len = strlen(str);
        const char * p1 = this->parseSchema(url, str, len, e);
        if ( !p1 ) return false;
        
        len = len - (p1 - str);
        const char *p2 = this->parseUserPass(url, p1, len, e);
        if ( !p2 ) return false;

        len = len - (p2 - p1);
        const char * p3 = this->parseHostPort(url, p2, len, e);
        if ( !p3 ) return false;

        len = len - ( p3 - p2 );
        const char *p4 = this->parsePath(url, p3, len, e);
        if ( !p4 ) return false;

        len = len - ( p4 - p3 );
        const char * p5 = this->parseQuery(url, p4, len, e);
        if ( !p5 ) return false; 

        return true;
    } // end URLParser::parse

    const char * URLParser::parseSchema(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p = this->find(str, len, ":/", 2);
        if ( p == nullptr ) return str;   // not found

        if ( p[0] == ':' && p[1] == '/' && p[2] == '/' ) {
            url->m_schema.assign(str, p - str);
            return p + 3;
        }  else {
            return str;
        }
    }
    
    const char * URLParser::parseUserPass(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p0 = this->find(str, len, "@/", 2 );
        if ( !p0 || p0[0] != '@' ) return str;  // 没有user部分，直接返回str
        
        // 截取出user:pass部分, 再解析user和pass
        int len1 = p0 - str;
        const char * p1 = this->find(str, len1, ":", 1);
        if ( p1 && p1[0] == ':') {
            url->m_user.assign(str, p1 - str);
            url->m_password.assign(p1 + 1, p0 - (p1 + 1));
        } else {
            url->m_user.assign(str, p0 - str);
            url->m_password = "";
        }
        return p0 + 1;
    }

    const char * URLParser::parseHostPort(URL * url, const char * str, int len, err::Error * e)
    {
        const char *p0 = this->find(str, len, "/?#", 1);
        if ( p0 && p0[0] != '/' ) return nullptr;   // 找到，但不是/，是为格式错误
        if ( !p0 ) p0 = str + len;
        
        int len1 = p0 - str;  // 截取host:port部分
        int len2 = len1;      // host部分
        const char * p1 = this->rfind(str, len1, ":].", 3);
        if ( p1[0] == ':' ) {
            url->m_port.assign(p1 + 1, p0 - (p1 + 1));
            len2 = len1 - (p0 - p1);
        }
        else p1 = p0;  // 没有port，重新指向结尾

        // 解析host部分
        url->m_host.assign(str, len2);
        return p0;
    }
    const char * URLParser::parsePath(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p0 = this->find(str, len, "#?", 2);
        if ( !p0 ) p0 = str + len;

        const char * p1 = this->find(str, len, "/", 1);
        if ( p1 ) {
            url->m_path.assign(p1, p0 - p1);
        } else {
            url->m_path.assign(str, p0 - str);
        }

        return p0;
    }

    const char * URLParser::parseQuery(URL * url, const char * str, int len, err::Error * e)
    {
        const char * p0 = this->find(str, len, "?", 2);
        if ( p0 ) {
            url->m_query.assign(p0 + 1);
            return str + len;
        } else {
            return str;
        }
    }

    const char * URLParser::find(const char * str, int len1, const char chars[], int len2)
    {
        for( int i = 0; i < len1; ++i ) {
            for (int t = 0; t < len2; ++t) {
                if ( str[i] == chars[t]) return str + i;
            }
        }
        return nullptr;
    }

    const char * URLParser::rfind(const char * str, int len1, const char chars[], int len2)
    {
        // 反向查找
        for( int i = len1 - 1; i >= 0; --i ) {
            for (int t = 0; t < len2; ++t) {
                if ( str[i] == chars[t]) return str + i;
            }
        }
        return nullptr;
    }

    std::string  URL::toString() const {
            std::string str;
            str.reserve(1024);
            if ( !m_schema.empty() ) str.append(m_schema).append("://");
            if ( !m_host.empty() ) {
                if ( !m_user.empty() ) {
                    str.append(m_user);
                    if ( !m_password.empty() ) str.append(1, ':').append(m_password);
                    str.append(1, '@');
                }
                str.append(m_host);
                if ( !m_port.empty() ) str.append(1, ':').append(m_port);
            }
            if ( !m_path.empty() ) {
                if ( m_path[0] != '/') str.append(1, '/');
                str.append(m_path);
            }
            if ( !m_query.empty() ) {
                if ( m_path.empty() ) str.append(1, '/');
                str.append(m_query);
            }
            return str;
        }

} // end namespace net
