#pragma once 

#include <sym/symdef.h>
#include <sym/error.h>
#include <sym/chrono.h>
#include <sym/thread.h>
#include <sym/network.h>
#include <sym/io.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>

BEGIN_SYM_NAMESPACE

/// 包含同步非阻塞IO相关操作的名称空间。
namespace nio
{
     enum {
        selectNone     = 0,
        selectRead     = 1,
        selectWrite    = 2,
        selectTimeout  = 4,
        selectError    = 8
    };

    /// I/O多路复用选择器。在Linux环境下，基于EPOLL实现。
    class Selector {
    public:
        class Event
        {
        private:
            int    m_fd      { -1 };
            int    m_sevents { selectNone };
            void * m_data    { nullptr };
        public:
            Event() {}
            Event(int fd, int sevents, void *data)
                : m_fd(fd), m_sevents(sevents), m_data(data) {}

            int    sevents() const {  return m_sevents; }
            void   sevents(int sevents) { m_sevents = sevents; }
            void * data() const   { return m_data; }
            void   data(void *dat) { m_data = dat; }
        };
        
        using EventMap = std::unordered_map<int, Event>;
        using EventVec = std::vector<Event>;
        using EpollEventVec = std::vector<epoll_event>;
    private:
        int            m_epfd;
        EventMap       m_events;
        EventVec       m_revents;
        EpollEventVec  m_epevents;
    public:
        /// \brief 默认构造函数。
        /// 
        ///     在Linux系统下，创建并初始化一个epoll fd.
        Selector();
        ~Selector();

        bool add(int fd, int events, void * data, err::Error * e = nullptr);
        bool remove(int fd, err::Error * e = nullptr);
        bool set(int fd, int events, err::Error * e = nullptr);
        bool cancel(int fd, int events, err::Error *e = nullptr);
        int  wait(int ms, err::Error * e = nullptr);
        Event * revents(int i);
    }; // end class Selector

    class SocketChannel;
    class SocketListener;

    enum class EnumIoType
    {
        ioSocketChannel,
        ioSocketListener
    };

    /**
     * @brief 简单的Socket服务端类。
     * 
     * 包含以下基本特性：
     *      1. 仅支持单线程多路复用I/O操作，不支持多线程(除wakeup外所以方法必须在同一线程执行)，以简化逻辑提升性能。
     *      2. 支持多个Socket TCP端口或UNIX Socket监听。
     *      3. 监听器持续获取新连接，直至监听器关闭。
     *      4. 所有获取到的连接，将持续接收消息，直至连接关闭。
     */
    class SimpleSocketServer {
        class ImplClass;
        ImplClass * m_impl;

    public:
        typedef std::function<void (int sfd, int cfd, const net::Address * remote )> ListenerCallback;
        typedef std::function<void (int fd, int status, io::ConstBuffer & buffer)> SendCallback;
        typedef std::function<void (int fd, int status, io::MutableBuffer & buffer)> RecvCallback;
        typedef std::function<void (int fd)>     CloseCallback; 
        typedef std::function<void (int status)> ServerCallback;
        typedef std::function<bool (int timer)>  TimerCallback;

        enum {
            statusOk     =  0,     ///< 正常状态
            statusError  = -1,     ///< 错误状态
            statusCancel = -2,     ///< 取消状态，如因连接关闭，队列里的读写操作都会cancel
            statusIdle   =  0      ///< 服务空闲状态，与statusOk相同
        };

    public:
        SimpleSocketServer();
        ~SimpleSocketServer();

        int   acceptChannel(int fd, const RecvCallback & rcb, const SendCallback & scb, const CloseCallback &ccb, err::Error * e = nullptr);

        int   addListener(const net::Address &loc, const ListenerCallback & callback, err::Error * e = nullptr);

        bool  beginReceive(int channel, io::MutableBuffer & buffer, err::Error *e = nullptr);

        void  exitLoop();

        bool  closeChannel(int fd, err::Error * e = nullptr);
        bool  closeListener(int fd, err::Error * e = nullptr);

        bool  send(int channel, io::ConstBuffer & buffer, err::Error * e = nullptr);
        
        void  setIdleInterval(int interval); 
        void  setServerCallback(const ServerCallback & callback);
        
        bool  shutdownChannel(int fd, int how, err::Error * e = nullptr);

        bool  run(err::Error * e);
        bool  wakeup();
    }; // end class SimpleSocketServer

} // end namespace nio

#include <unordered_map>

namespace nio 
{
    /// IO对象基础类。子类包括SocketChannel和SocketListener。
    class IoBase {
    private:
        EnumIoType m_type;

    public:
        IoBase( EnumIoType type ) : m_type(type) {}

        EnumIoType type() const { return m_type; }
    }; // end class IoBase

    class SocketChannel : public IoBase {
        using InputBufferQueue = std::list<io::MutableBuffer> ;
        using OutputBufferQueue = std::list<io::ConstBuffer> ;
    private:
        net::Socket       m_sock;
        int               m_shutFlags  { 0 };
        InputBufferQueue  m_inputBuffers;
        OutputBufferQueue m_outputBuffers;

    public:
        SocketChannel() : IoBase(EnumIoType::ioSocketChannel)  {}
        SocketChannel(int fd) : IoBase(EnumIoType::ioSocketChannel), m_sock(fd) {}
        ~SocketChannel() { if (m_sock.fd() >= 0) m_sock.close(); }

        int  fd() const { return m_sock.fd(); }
        bool open(const net::Address & remote, int timeout, err::Error * e = nullptr);
        bool close(err::Error * e = nullptr);

        /// 执行一次recv操作，无论是否收到数据。收到的数据写入队列缓存。
        int  receive(err::Error * e = nullptr);

        /// 执行一次或多次recv操作，直到收到Limit大小的数据，或者超时。
        int  receiveN(io::MutableBuffer & buffer, int timeout, err::Error * e = nullptr);

        /// 执行一次或多次recv操作，直到受收到任意数量的字节(>0)，或者接收失败或超时。
        int  receiveSome(io::MutableBuffer & buffer, int timeout, err::Error * e = nullptr);
        
        /// 对当前发送队列缓存执行一次send操作，无论是否有数据发送出去都将返回。
        int  send(err::Error * e = nullptr);

        /// 执行一次或多次send操作，直到limit大小的数据被发送，或者超时，才返回。
        int  sendN(io::ConstBuffer & buffer, int timeout, err::Error *e = nullptr);

        /// 执行一次或多次send操作，直到有任意数量(>0)数据被发送,或者超时，才返回。
        int  sendSome(io::ConstBuffer & buffer, int timeout, err::Error *e = nullptr);
        
        bool shutdown(int how, err::Error *e = nullptr);
        int  shutdownFlags() const { return m_shutFlags; }

        void  pushInputBuffer(io::MutableBuffer & buf) { m_inputBuffers.push_back(buf); }
        void  pushOutputBuffer(io::ConstBuffer & buf)  { 
            if ( buf.data() == nullptr ) {
                SYM_TRACE("PUSH EMPTY BUFFER");
            }
            m_outputBuffers.push_back(buf); 
        }

        io::MutableBuffer * peekInputBuffer() { return m_inputBuffers.empty()?nullptr:&m_inputBuffers.front(); }
        io::ConstBuffer * peekOutputBuffer()  { return m_outputBuffers.empty()?nullptr:&m_outputBuffers.front(); }
        
        void popInputBuffer() { if ( !m_inputBuffers.empty()) m_inputBuffers.pop_front(); }
        void popOutputBuffer() { if ( !m_outputBuffers.empty()) m_outputBuffers.pop_front(); }
        
    private:
        /// 等待特定的事件，events取值selectRead/selectWrite组合，timeout单位毫秒（-1不超时）
        /// 返回值是selectRead/selectWrite/selectError的组合，或者0表示超时，-1表示Poll异常
        int  wait(int events, int timeout, err::Error * e = nullptr);
    }; // end class SocketChannel

    class SocketListener : public IoBase {
    private:
        net::Socket m_sock;
    public:
        SocketListener() : IoBase( EnumIoType::ioSocketListener ) {}
        ~SocketListener() { if ( m_sock.fd() >= 0 ) m_sock.close();}
        int fd() const { return m_sock.fd(); }
        bool open(const net::Address & localAddr, err::Error * e= nullptr);
        int acceptFd(net::Address * remote, err::Error * e); 
    }; // end class SocketListener

    class SimpleSocketServer::ImplClass {
    public:
        struct ListenerEntry {
            SocketListener * listener;
            ListenerCallback callback;
        };
        struct ChannelEntry {
            SocketChannel * channel;
            RecvCallback    recvCb;
            SendCallback    sendCb;
            CloseCallback   closeCb;
        };

        using ChannelMap  = std::unordered_map<int, ChannelEntry>;
        using ListenerMap = std::unordered_map<int, ListenerEntry>;

        using Request = std::function<void ()>;

    public:
        Selector       m_selector;
        ListenerMap    m_listenerMap;
        ChannelMap     m_channelMap;
        int            m_idleInterval {-1};
        int            m_exitloop { false };
        ServerCallback m_serverCb;
        std::queue<Request> m_requestQueue;

    public:
        ImplClass()   {}
        ~ImplClass()  {}

        SocketChannel * getChannel(int fd);

        void onListenerEvent(Selector::Event * event);
        void onChannelEvent(Selector::Event * event);
        void onServerIdle();

        bool pushShutdownRequest(int channel, int how);
        bool pushChannelCloseRequest(int channel);

        bool hasRequest() const { return !m_requestQueue.empty(); }
        Request popRequest()  { 
            Request r = m_requestQueue.front(); 
            m_requestQueue.pop();
            return r;
        }

    private:
        void onChannelWritable(ChannelEntry & entry);
        void onChannelReadable(ChannelEntry & entry);
        void onChannelError(ChannelEntry & entry);
    }; // end classs SimpleSocketServer::ImplClass

} // end namespace nio

namespace nio
{
    inline 
    Selector::Selector() : m_epfd(-1)
    {
        m_epfd = epoll_create1(EPOLL_CLOEXEC);
        assert(m_epfd >= 0);
    }

    inline 
    Selector::~Selector() {
        if ( m_epfd >= 0 ) ::close(m_epfd);
    }

    inline 
    bool Selector::add(int fd, int events, void *data, err::Error * e)
    {
        struct epoll_event evt;
        evt.data.fd = fd;
        evt.events = 0;
        if ( events & selectRead  ) evt.events |= EPOLLIN;
        if ( events & selectWrite ) evt.events |= EPOLLOUT;

        int rv = ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &evt);
        if ( rv == -1 ) {
            if ( e ) *e = err::Error(errno, err::dmSystem);
            return false;
        }
        
        // 放入map
        Event event(fd, events, data);
        auto r = m_events.insert(std::make_pair(fd, event));
        assert( r.second );

        // epoll event列表+1
        if ( m_epevents.size() == m_epevents.capacity() ) {
            m_epevents.reserve(m_epevents.size() + 1024);
        }
        m_epevents.resize( m_epevents.size() + 1);

        // revents列表递增
        if ( m_revents.size() == m_revents.capacity() ) {
            m_revents.reserve(m_revents.size() + 1024);
        }
        m_revents.resize( m_revents.size() + 1);

        return true;
    }

    inline 
    bool Selector::remove(int fd, err::Error * e)
    {
        m_events.erase(fd);
        int rv = epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
        if ( rv != 0 ) {
            if ( e ) *e = err::Error(errno, err::dmSystem);            
            return false;
        }

        // revents数组递减
        m_revents.resize(m_revents.size() - 1);
        m_epevents.resize(m_epevents.size() - 1);
        return true;
    }

    inline 
    bool Selector::set(int fd, int events, err::Error * e) 
    {
        auto it = m_events.find(fd);
        assert( it != m_events.end());

        Event & event = it->second;
        
        int sevents = event.sevents() | events;

        struct epoll_event evt;
        evt.data.fd = fd;
        evt.events  = 0;
        if ( sevents & selectRead ) evt.events |= EPOLLIN;
        if ( sevents & selectWrite ) evt.events |= EPOLLOUT;

        int rv = epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &evt);
        if ( rv == -1 )  {
            if ( e ) *e = err::Error(errno, err::dmSystem);            
            return false;
        }

        event.sevents( sevents );
        return true;
    }

    inline 
    bool Selector::cancel(int fd, int events, err::Error * e) 
    {
        auto it = m_events.find(fd);
        assert( it != m_events.end());

        Event & event = it->second;
        
        int sevents = event.sevents() &  (~events);

        struct epoll_event evt;
        evt.data.fd = fd;
        evt.events  = 0;
        if ( sevents & selectRead ) evt.events |= EPOLLIN;
        if ( sevents & selectWrite ) evt.events |= EPOLLOUT;

        int rv = epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &evt);
        if ( rv == -1 )  {
            if ( e ) *e = err::Error(errno, err::dmSystem);            
            return false;
        }

        event.sevents( sevents );
        return true;
    }

    inline 
    int Selector::wait(int ms, err::Error * e)
    {
        int rv = epoll_wait(m_epfd, &m_epevents[0], m_epevents.size(), ms);
        if ( rv > 0 ) {
            for ( int i = 0;  i < rv; ++i ) {
                struct epoll_event & epevt = m_epevents[i];
                int fd = epevt.data.fd;
                int events = 0;
                if ( epevt.events & EPOLLIN ) events |= selectRead;
                if ( epevt.events & EPOLLOUT) events |= selectWrite;
                if ( epevt.events & EPOLLERR) events |= selectError;

                Event & event = m_events[fd];
                m_revents[i] = Event(fd, events, event.data());
            }
            return rv;
        } else if ( rv < 0) {
            if ( e ) *e = err::Error(errno, err::dmSystem);
        }

        return rv;
    }

    inline 
    Selector::Event * Selector::revents(int i) {
        return &m_revents[i];
    }

} // end namespace nio


namespace nio
{
    inline 
    int SocketChannel::wait(int events, int timeout, err::Error * e)
    {
        struct pollfd pfd;
        pfd.fd = m_sock.fd();
        pfd.events = 0;
        if ( events & selectRead ) pfd.events |= POLLIN;
        if ( events & selectWrite) pfd.events |= POLLOUT;
        int r = 0;
        int64_t now = chrono::now();
        int64_t now1 = now;
        int     timeout1 = timeout;
        int     revents = 0;
        while (1) {
            r = ::poll(&pfd, 1, timeout1);
            if ( r >= 0 ) {
                if ( pfd.revents & POLLIN ) revents |= selectRead;
                if ( pfd.revents & POLLOUT) revents |= selectWrite;
                if ( pfd.revents & POLLERR) revents |= selectError;
                break;
            } else {
                int eno = errno;
                if ( eno == EINTR ) {
                    if ( timeout > 0 ) {
                        now1 = chrono::now();
                        timeout1 = timeout - (int)((now1 - now) / 1000);
                        // 为了避免极端情况，差一点点超时时被中断，但上面取时间再计算后变得超时
                        // 这里也是需要判断一把，否则timeout1<0，循环出去就变成永不超时了
                        if ( timeout1 < 0 ) {
                            revents = 0; 
                            break;   // 设置返回超时并推出循环
                        }
                    }
                    continue;
                }
                else {
                    // 这里就是poll完全异常了
                    revents = -1;
                    if( e ) *e = err::Error(eno, err::dmSystem);
                    break;
                }
            } // if r
        } // while
        return revents;
    }

    inline 
    int SocketChannel::receive(err::Error * e)
    {
        if ( m_inputBuffers.empty() ) return 0;
        auto & buffer = m_inputBuffers.front();
        int remain = buffer.limit() - buffer.size();
        
        err::Error e2;
        ssize_t n = m_sock.receive(buffer.data() + buffer.size(), remain, &e2);
        if ( n > 0 ) {
            buffer.resize( buffer.size() + n );
            remain = buffer.limit() - buffer.size();
            return n;
        } else if ( n == 0 ) {
            return 0;     // 没读到消息
        } else {
            return -1;    // 读取错误
        }
    }

    inline 
    int SocketChannel::receiveSome(io::MutableBuffer & buffer, int timeout, err::Error * e)
    {
        int remain = buffer.limit() - buffer.size();
        int total = 0;
        assert ( remain >= 0 );

        char * ptr = buffer.data() + buffer.size();
        err::Error e1;

        total = m_sock.receive(ptr, remain, &e1);
        if ( total == 0 ) {
            // 没有收到消息，则等待
            int w = this->wait(selectRead, timeout, &e1);
            if ( w & selectRead ) total = m_sock.receive(ptr, remain, e);
            else if ( w & selectError ) {
                if ( e ) *e = err::Error(-1, "poll read error event");
                total = -1;
            }
            else if ( w < 0 ) {
                if ( e ) *e = e1;
                total = -1;   // 等待错误等同与接收错误
            }
        }
        if ( total > 0 ) buffer.resize(buffer.size() + total);
        return total;    // >0 收到数据，=0超时，<0错误。
    }

    inline
    int SocketChannel::receiveN(io::MutableBuffer & buffer, int timeout, err::Error * e)
    {
        int remain = buffer.limit() - buffer.size();
        int total  = 0;
        assert ( remain >= 0 );

        char * ptr = buffer.data() + buffer.size();
        
        int64_t now = chrono::now();
        int64_t now1 = now;
        int     timeout1 = timeout;
        err::Error e1;

        do {
            e1.clear();
            int n = m_sock.receive(ptr + total, remain - total, e);  // 收一次
            if ( n > 0 ) {
                total += n;
                remain -= n;
                continue;    // 收到了n个字节
            } else if ( n == 0 ) {
                int w = this->wait(selectRead, timeout1, &e1);   // 没有收到，就等
                if ( w & selectRead ) {
                    if ( timeout > 0 ) {
                        now1 = chrono::now();
                        timeout1 = timeout - (int)((now1 - now) / 1000); 
                        if ( timeout1 < 0 ) timeout1 = 0; // 这里避免极端情况
                    }
                    continue;        // 有数据了，继续收
                } else if ( w & selectError ) {
                    if ( e ) *e = err::Error(-1, "poll read error event");
                    total = -1;
                    break;
                } else if ( w == 0 ) {
                    total = 0; 
                    break;    // 超时，退出
                } else {     //  ( w < 0 ) 
                    total = -1;
                    if ( e ) *e = e1;
                    break;    // 等待错误
                } // if w
            } else {   // n < 0 , receive error
                total = -1;
                break;
            } // if n
        } while ( remain > 0 );

        if ( total > 0 ) buffer.resize( buffer.size() + total );
        return total;
    } // receiveN

    inline
    int SocketChannel::send(err::Error *e)
    {
        if ( m_outputBuffers.empty()) {
            SYM_TRACE("SocketChannel::send, no send buffer");
            return 0;  // 没有可发数据
        }
        auto & buffer = m_outputBuffers.front();
        int remain = buffer.limit() - buffer.position();
        
        int n = m_sock.send(buffer.data() + buffer.position(), remain, e);
        if ( n > 0 ) {
            buffer.position( buffer.position() + n );
            remain = buffer.limit() - buffer.position();
            SYM_TRACE_VA("SocketChannel::send, data sent, %d", n);
            return n;
        } else if ( n == 0 ) {
            return 0;    // 没有发出，输出缓存已满
        } else {
            return -1;
        }
    }
    
    inline
    int SocketChannel::sendSome(io::ConstBuffer & buffer, int timeout, err::Error * e)
    {
        int remain = buffer.limit() - buffer.position();
        int total  = 0;
        assert ( remain >= 0 );

        const char * ptr = buffer.data() + buffer.position();
        
        total = m_sock.send(ptr, remain, e);
        if ( total == 0 ) {
            // 没有数据被发送，需要等待
            err::Error e1;
            int w = this->wait(selectWrite, timeout, &e1);
            if ( w & selectWrite ) {
                total = m_sock.send(ptr, remain, e);   // 执行一次发送
            } else if ( w & selectError) {
                if ( e ) *e = err::Error(-1, "poll write error event");
                total = -1;
            }  
            else if ( w == 0 ) {
                total = 0;  // 超时
            } else {
                total = -1;
                if ( e ) *e = e1;
            }
        }

        if ( total > 0 ) buffer.position(buffer.position() + total);
        return total;
    }

    inline
    int SocketChannel::sendN(io::ConstBuffer & buffer, int timeout, err::Error * e)
    {
        int remain = buffer.limit() - buffer.position();
        int total  = 0;
        assert ( remain >= 0 );

        const char * ptr = buffer.data() + buffer.position();
        
        int64_t now = chrono::now();
        int64_t now1 = now;
        int     timeout1 = timeout;
        err::Error e1;

        do {
            e1.clear();
            int n = m_sock.send(ptr + total, remain - total, e);  // 发一次
            if ( n > 0 ) {
                total += n;
                remain -= n;
                continue;    // 发出了n个字节
            } else if ( n == 0 ) {
                int w = this->wait(selectWrite, timeout1, &e1);   // 没有发出，就等
                if ( w & selectWrite ) {
                    if ( timeout > 0 ) {
                        now1 = chrono::now();
                        timeout1 = timeout - (int)((now1 - now) / 1000); 
                        if ( timeout1 < 0 ) timeout1 = 0;
                    }
                    continue;        // 有数据了，继续发
                } else if ( w & selectError ) {
                    if ( e ) *e = err::Error(-1, "poll write error event");
                    total = -1;
                    break;
                } else if ( w == 0 ) {
                    total = 0; 
                    break;    // 超时，退出
                } else {     //  ( w < 0 ) 
                    total = -1;
                    if ( e ) *e = e1;
                    break;    // 等待错误
                } // if w
            } else {   // n < 0 , send error
                total = -1;
                break;
            } // if n
        } while ( remain > 0 );

        if ( total > 0 ) buffer.position( buffer.position() + total );
        return total;
    } // receiveN

    inline 
    bool SocketChannel::shutdown(int how, err::Error *e) 
    { 
        m_shutFlags |= how; 
        return m_sock.shutdown(how, e); 
    }

    inline 
    bool SocketChannel::close(err::Error * e )
    {
        m_shutFlags = net::shutdownBoth;
        return m_sock.close(e);
    }

    inline
    bool SocketChannel::open(const net::Address & remote, int timeout, err::Error * e)
    {
        assert(m_sock.fd() == -1);
        bool isok = m_sock.create(remote.af(), SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, e);
        if ( !isok ) return false;

        isok = m_sock.connect(remote, e);
        if (!isok ) {
            m_sock.close();
            return false;
        }

        int r = this->wait(selectWrite, timeout, e);
        if ( r > 0 ) {
            return true;
        } else {
            // wait error
            m_sock.close();
            return false;
        }
    }
    
    inline 
    bool SocketListener::open(const net::Address & localAddr, err::Error * e)
    {
        bool isok;
        isok = m_sock.create(localAddr.af(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, e);
        if ( !isok ) return false;

        net::SocketOptReuseAddr soReuseAddr(m_sock, true);

        isok = m_sock.bind(localAddr, e);
        if ( !isok ) {
            m_sock.close();
            return false;
        }

        isok = m_sock.listen(e);
        if ( !isok ) {
            m_sock.close();
            return false;
        }

        return true;
    }

    inline
    int SocketListener::acceptFd(net::Address * remote, err::Error * e)
    {
        return m_sock.accept(remote, SOCK_NONBLOCK | SOCK_CLOEXEC, e);
    }
    
    inline 
    bool SimpleSocketServer::ImplClass::pushChannelCloseRequest(int fd)
    {
        auto request = [this, fd]() {

            // 获取并关闭channel
            SocketChannel * channel = this->getChannel(fd);
            if ( channel == nullptr ) return;  // already closed

            this->m_selector.remove( fd );   // remove fd from selector synchronized
            channel->close();

            SYM_TRACE_VA("[trace] DO_CLOSE_RQUEST, channel: %d", fd);
            auto itEntry = this->m_channelMap.find(fd);
            assert(itEntry != this->m_channelMap.end());
            auto & entry = itEntry->second;
            entry.closeCb(fd);
            this->m_channelMap.erase(fd);
        };

        m_requestQueue.push(request);
    }

    inline 
    bool SimpleSocketServer::ImplClass::pushShutdownRequest(int channel, int how)
    {
        auto request = [this, channel, how]() {
            SYM_TRACE_VA("[trace] DO_SHUT_RQUEST, channel: %d, how: %d", channel, how);
            auto itEntry = this->m_channelMap.find(channel);
            assert(itEntry != this->m_channelMap.end());
            auto & entry = itEntry->second;
            if ( how & net::shutdownWrite ) {
                io::ConstBuffer * buf;
                while ( buf = entry.channel->peekOutputBuffer() ) {
                    entry.sendCb(channel, statusCancel, *buf);
                    entry.channel->popOutputBuffer();
                }
            }
            if ( how & net::shutdownRead ) {
                io::MutableBuffer * buf;
                while ( buf = entry.channel->peekInputBuffer() ) {
                    entry.recvCb(channel, statusCancel, *buf);
                    entry.channel->popInputBuffer();
                }
            }
        };
        m_requestQueue.push(request);
    }

    inline 
    void SimpleSocketServer::ImplClass::onChannelError(ChannelEntry & entry)
    {
        // channel事件监听发生异常，视作读写全部失败，所有读写操作都将执行异常回调。
        int fd = entry.channel->fd();
        io::ConstBuffer * ob ;
        while ( ob = entry.channel->peekOutputBuffer() ) {
            entry.sendCb(fd, statusError, *ob);
            entry.channel->popOutputBuffer();
        }

        io::MutableBuffer * ib;
        while ( ib = entry.channel->peekInputBuffer() ) {
            entry.recvCb(fd, statusError, *ib);
            entry.channel->popInputBuffer();
        }
    }

    inline 
    void SimpleSocketServer::ImplClass::onChannelReadable(ChannelEntry & entry)
    {
        SocketChannel * channel = entry.channel;
        ssize_t recvSize = 0;

        // 循环接收，直到没有数据可收
        while ( ( recvSize = channel->receive() ) > 0 ) {
            io::MutableBuffer * buf = channel->peekInputBuffer();
            SYM_TRACE_VA("[trace] ON_READABLE, received: %d, limit: %d, size: %d", 
                (recvSize), buf->limit(), buf->size());
            if ( buf->size() == buf->limit() ) {
                entry.recvCb(channel->fd(), statusOk, *buf);
                channel->popInputBuffer();
                break;  // 回调执行后不再继续读，因为如果收到的数据异常，再回调中channel已经被执行close操作。
            }
        } // end while

        // 接收失败，回调
        if ( recvSize < 0 )  {
            SYM_TRACE_VA("[error] ON_READABLE_ERROR, received: %d", (recvSize));
            entry.recvCb(channel->fd(), statusError, *channel->peekInputBuffer());
            channel->popInputBuffer();
            m_selector.cancel(channel->fd(), selectRead);
        }
    }

    inline 
    void SimpleSocketServer::ImplClass::onChannelWritable(ChannelEntry & entry)
    {
        SocketChannel * channel = entry.channel;
        ssize_t n = channel->send();
        io::ConstBuffer * buf = channel->peekOutputBuffer();
        assert( buf );

        SYM_TRACE_VA("SIMP_SOCK_SERVER::onChannelWritable, channel; %d, sent; %d", channel->fd(), n);

        if ( n >= 0 ) {
            // 需要发送的数据发送完成, 执行回调，并将该缓存发送队列中取出
            if ( buf->position() == buf->limit() ) {
                entry.sendCb(channel->fd(), statusOk, *buf);
                channel->popOutputBuffer();
            }
        } else {
            // 发送失败, 执行失败回调。只回调输出当前buffer，其余buffer在shutdownWrite时逐个返回
            entry.sendCb(channel->fd(), statusError, *buf);
            channel->popOutputBuffer();
        }
            
        // 如果没有缓存，则取消selectWrite事件, 无论这次发送正常或者失败。
        if ( !channel->peekOutputBuffer() ) {
            m_selector.cancel(channel->fd(), selectWrite);
        }
    }

    inline 
    void SimpleSocketServer::ImplClass::onChannelEvent(Selector::Event * event)
    {
        SocketChannel * channel = (SocketChannel*)event->data();
        auto itEntry = m_channelMap.find( channel->fd()) ;
        assert( itEntry != m_channelMap.end());

        if ( event->sevents() & selectWrite ) {
            this->onChannelWritable(itEntry->second);
        }
        if ( event->sevents() & selectRead ) {  
            this->onChannelReadable(itEntry->second);
        }
        if ( event->sevents() & selectError ) {
            this->onChannelError(itEntry->second);
        }
    }

    inline 
    void SimpleSocketServer::ImplClass::onListenerEvent(Selector::Event * event)
    {
        SocketListener * listener = (SocketListener*)event->data();
        auto itEntry = m_listenerMap.find(listener->fd());
        assert ( itEntry != m_listenerMap.end());

        if ( event->sevents() & selectRead ) {
            int cfd;
            net::Address remote;
            err::Error error;

            while ( cfd = listener->acceptFd(&remote, &error) ) {
                if ( cfd >= 0 ) {
                    itEntry->second.callback(listener->fd(), cfd, &remote);
                } else if ( !error ) {
                    SYM_TRACE("[trace] no more connection to accept");
                    break;  // 所有排队的连接都已获取
                } else {
                    // 获取连接失败, 执行异常回调，回调过程通常关闭该监听
                    SYM_TRACE_VA("[error] accept connection failed, %s", error.message());
                    itEntry->second.callback(listener->fd(), -1, nullptr);
                    break;
                }
            }
        } 
        if ( event->sevents() & selectError ) {
            itEntry->second.callback(listener->fd(), -1, nullptr);
        } // end if 
    } 

    inline 
    void SimpleSocketServer::ImplClass::onServerIdle()
    {
        if ( m_serverCb ) m_serverCb(statusIdle);
    }

    inline 
    SocketChannel * SimpleSocketServer::ImplClass::getChannel(int fd)
    {
        auto it = m_channelMap.find(fd);
        if ( it == m_channelMap.end() ) return nullptr;
        else return it->second.channel;
    }
} // end namespace nio


namespace nio 
{
    inline 
    SimpleSocketServer::SimpleSocketServer() 
        : m_impl(new ImplClass)
    {
    }

    inline 
    SimpleSocketServer::~SimpleSocketServer()
    {
        if ( m_impl ) {
            delete m_impl;
            m_impl = nullptr;
        }
    }

    inline
    int SimpleSocketServer::acceptChannel (
        int fd, 
        const RecvCallback &rcb, 
        const SendCallback & scb, 
        const CloseCallback & ccb, 
        err::Error * e )
    {
        std::unique_ptr<SocketChannel> ptrChannel(new SocketChannel(fd));
        bool isok = m_impl->m_selector.add(fd, selectNone, ptrChannel.get(), e);
        assert( isok );

        auto it = m_impl->m_channelMap.find(fd);
        assert ( it == m_impl->m_channelMap.end() );
        ImplClass::ChannelEntry entry { ptrChannel.release(), rcb, scb, ccb};
        m_impl->m_channelMap[fd] = entry;
        return fd;
    }

    inline 
    int SimpleSocketServer::addListener(const net::Address & localAddr, const ListenerCallback & cb, err::Error * e)
    {
        std::unique_ptr<SocketListener> ptrListener(new SocketListener());
        bool isok = ptrListener->open(localAddr, e);
        if ( !isok ) return false;

        // 监听成功, 注册到Selector，然后放入表中，最后返回监听器描述字表示当前监听器的ID。
        //
        int fd = ptrListener->fd();
        isok = m_impl->m_selector.add(fd, selectRead, ptrListener.get(), e);
        assert( isok );

        auto it = m_impl->m_listenerMap.find(ptrListener->fd());
        assert( it == m_impl->m_listenerMap.end());

        ImplClass::ListenerEntry entry{ptrListener.release(), cb};
        m_impl->m_listenerMap[ fd ] = entry;
        return fd;
    }

    inline 
    bool  SimpleSocketServer::beginReceive(int channel, io::MutableBuffer & buffer, err::Error *e)
    {
        SocketChannel * pch = m_impl->getChannel(channel);
        assert( pch );

        pch->pushInputBuffer(buffer);
        return m_impl->m_selector.set(channel, selectRead, e);
    }

    inline
    bool SimpleSocketServer::closeChannel(int fd, err::Error * e) 
    {
        m_impl->pushChannelCloseRequest(fd);
        return true;
    }

    inline 
    void SimpleSocketServer::exitLoop() {
        m_impl->m_exitloop = true;
    }

    inline 
    void SimpleSocketServer::setIdleInterval(int sec) 
    {
        m_impl->m_idleInterval = sec * 1000;
    }

    inline
    void SimpleSocketServer::setServerCallback(const ServerCallback & cb)
    {
        m_impl->m_serverCb = cb;
    }

    inline
    bool SimpleSocketServer::run(err::Error * e)
    {
        m_impl->m_exitloop = false;
        while ( !m_impl->m_exitloop ) {
            // 执行异步请求
            while ( m_impl->hasRequest() ) {
                auto request = m_impl->popRequest();
                request();
            }

            int r = m_impl->m_selector.wait(m_impl->m_idleInterval, e);
            if ( r > 0 ) {
                for ( int i = 0; i < r; ++i ) {
                    Selector::Event * event = m_impl->m_selector.revents(i);
                    IoBase * base = (IoBase*)event->data();
                    if ( base->type() == EnumIoType::ioSocketChannel) {
                        m_impl->onChannelEvent(event);
                    } else if ( base->type() == EnumIoType::ioSocketListener) {
                        m_impl->onListenerEvent(event);
                    } else {
                        assert("unknown io type" == nullptr);
                    }
                } // end for
            }
            else if ( r == 0 ) {
                m_impl->onServerIdle();
                continue;
            }
            else {
                return false;
            }
        } // end while

        return true;
    } // SimpleSocketServer::run

    inline
    bool SimpleSocketServer::send(int channel, io::ConstBuffer & buffer, err::Error * e)
    {
        auto it = m_impl->m_channelMap.find(channel);
        if ( it == m_impl->m_channelMap.end()) {
            if ( e ) *e = err::Error(-1, "channel id not exists");
            return  false;
        }

        // 将buffer放入channel的输出队列，然后通知selector监听该channel的可写事件。
        // 这里必须把buffer放入队列，按顺序send，不能先尝试发送该缓存消息，
        // 不然当输出队列里还有发送缓存时，会造成消息错乱。
        ImplClass::ChannelEntry & entry = it->second;
        entry.channel->pushOutputBuffer(buffer);

        bool isok = m_impl->m_selector.set(channel, selectWrite, e);
        return isok;
    }

    inline
    bool SimpleSocketServer::shutdownChannel(int channel, int how, err::Error * e)
    {
        auto itChannelEntry = m_impl->m_channelMap.find(channel);
        if ( itChannelEntry == m_impl->m_channelMap.end() ) {
            SYM_TRACE_VA("[error] SHUT_CHANNEL_NOT_FOUND: channel: %d, how: %d", channel, how);
            if (  e ) *e = err::Error(-1, "unknown channel id");
            return false;
        }

        bool isok = itChannelEntry->second.channel->shutdown(how, e);
        if ( !isok ) return false;
        
        return m_impl->pushShutdownRequest(channel, how);  // 发送停用请求
    }

} // end namespace nio


END_SYM_NAMESPACE
