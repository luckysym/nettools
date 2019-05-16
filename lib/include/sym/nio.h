#pragma once 

#include <sym/error.h>
#include <sym/chrono.h>
#include <sym/algorithm.h>
#include <sym/thread.h>
#include <sym/network.h>
#include <sym/io.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <functional>
#include <memory>

/// 包含同步非阻塞IO相关操作的名称空间。
namespace nio
{
    const int nio_status_success = 0;
    const int nio_status_error   = 1;
    const int nio_status_timeout = 2;

    const int select_none     = 0;
    const int select_read     = 1;
    const int select_write    = 2;
    const int select_timeout  = 4;
    const int select_error    = 8;
    const int select_add      = 16;
    const int select_remove   = 32;
    
    const int init_list_size = 128;    ///< 列表初始化大小
    
    const int selopt_thread_safe = 1;  ///< selector support multi-thread

    /// selector event callback function type
    typedef void (*selector_event_proc)(int fd, int event, void *arg);

    /// 事件超时设置节点
    typedef struct select_expiration {
        int       fd;
        int       ops;
        int64_t   exp;
        int       inque;
    }  sel_expire_t;
    typedef alg::basic_dlink_node<sel_expire_t> sel_expire_node;

    typedef struct select_item {
        int    fd;
        int    events;    ///< current requested epoll events
        selector_event_proc  callback;
        void   *arg;
        sel_expire_node rd;
        sel_expire_node wr; 
    } sel_item_t;
    typedef alg::basic_dlink_node<sel_item_t> sel_item_node;

    typedef struct select_operation {
        int     fd;
        int     ops;      ///< requested operation events
        int64_t expire;
        selector_event_proc  callback;
        void   *arg;
        bool    async;    ///< is the operation asynchronous.
    } sel_oper_t;
    typedef alg::basic_dlink_node<sel_oper_t> sel_oper_node;

    typedef alg::basic_array<epoll_event> epoll_event_array;
    typedef alg::basic_array<sel_item_t>  sel_item_array;
    typedef alg::basic_dlink_list<sel_oper_t> sel_oper_list;
    typedef alg::basic_dlink_list<sel_expire_t> sel_expire_list;

    typedef struct select_event {
        int   fd;
        int   event;
        void *arg;
        selector_event_proc callback;
    } sel_event_t;
    typedef void (*selector_dispatch_proc)(sel_event_t * event, void * disparg);

    struct selector_epoll {
        mt::mutex_t      *reqlock;   ///< lock for request operation list(requests) accessing
        sel_oper_list     requests;  ///< request operation queue
        sel_item_array    items;     ///< registered items
        sel_expire_list   timeouts;  ///< timeout queue
        int               def_wait;  ///< default wait timeout
        int               count;     ///< total fd registered in epoll, event fd not involved
        selector_dispatch_proc  disp;  ///< event dispatcher
        void *            disparg;     ///< argument of disp proc

        int               evfd;     ///< event fd for nofitier
        int               epfd;     ///< epoll fd
        epoll_event_array events;   ///< receive the output events from epoll_wait
    };
    typedef struct selector_epoll selector_t;

    /// init and return the selector, returns null if failed
    bool selector_init(selector_t *sel, int options, err::error_t *err);

    /// init and return the selector, with custom event dispatcher
    bool selector_init(selector_t *sel, int options, selector_dispatch_proc disp, void *disparg, err::error_t *err);

    /// destroy the selector created by selector_create
    bool selector_destroy(selector_t *sel, err::error_t *err);

    /// add a socket fd and its callback function to the selector.
    bool selector_add(selector_t * sel, int fd, selector_event_proc cb, void *arg, err::error_t *err);

    // add a socket fd and its callback function to the selector async.
    bool selector_add_async(selector_t * sel, int fd, selector_event_proc cb, void *arg);

    /// remove socket from selector
    bool selector_remove(selector_t * sel, int fd, err::error_t *err);

    /// remove socket from selector 
    bool selector_remove_async(selector_t * sel, int fd);

    /// request events.
    bool selector_request(selector_t * sel, int fd, int events, int64_t expire, err::error_t *err);

    /// notify the selector waking up
    bool selector_wakeup(selector_t * sel, err::error_t *err);

    /// run the selector
    int  selector_run(selector_t *sel, err::error_t *err);

    namespace detail {

        inline
        void selector_sync_dispatcher(sel_event_t *event, void *disparg) {
            event->callback(event->fd, event->event, event->arg);
        }

    } // end namespace detail

    namespace epoll {

        sel_item_t * selector_add_internal(selector_epoll * sel, sel_oper_t * oper, err::error_t *e);
        bool selector_remove_internal(selector_epoll * sel, sel_oper_t *oper, err::error_t *e);
        bool selector_request_internal(selector_epoll *sel, sel_oper_t *oper, err::error_t *e);
        int  selector_run_internal(selector_epoll *sel, err::error_t *e);

    } // end namespace detail

    const int channel_state_closed  = 0;    ///< nio channel state closed
    const int channel_state_open    = 1;    ///< nio channel state open
    const int channel_state_opening = 2;    ///< nio channel state opening
    const int channel_state_closing = 3;    ///< nio channel state closing

    const int channel_event_received  = 1;
    const int channel_event_sent      = 2;
    const int channel_event_connected = 4;
    const int channel_event_accepted  = 8;
    const int channel_event_closed    = 16;
    const int channel_event_error     = 128;

    const int channel_shut_read  = net::sock_shut_read;
    const int channel_shut_write = net::sock_shut_write;
    const int channel_shut_both  = net::sock_shut_both;

    const int nio_flag_exact_size = 1;    ///< exact size for receiving or sending

    typedef struct nio_channel channel_t; ///< nio channel  type

    /// nio channel io event calllback function type
    typedef void (*channel_io_proc)(channel_t * ch, int event, void *io, void *arg);

    typedef void (*channel_read_callback)(channel_t * ch, int status, io::buffer_t * buf, void *arg);

    namespace detail {
        // nio buffer 结构
        typedef struct nio_operation {
            io::buffer_t buf;
            int          flags;
            int64_t      exp;
            union {
                channel_read_callback rdcb;
            } cb;
            void *       cbarg;
        } nio_oper_t;

        // nio buffer 链表节点类型
        typedef alg::basic_dlink_node<nio_oper_t> nio_oper_node_t;

        // nio buffer 队列类型
        typedef alg::basic_dlink_list<nio_oper_t> nio_oper_queue_t;

        // channel event callback registered into selector
        void channel_event_callback(int fd, int events, void *arg);
    }; // end namespace detail

    /// nio channel结构。
    struct nio_channel {
        int                 fd;      ///< channel文件描述字。
        int                 state;   ///< 当前channel状态，取值channel_state_*。
        selector_t *        sel;     ///< 当前channel关联的事件选择器。
        channel_io_proc     iocb;    ///< io回调函数。
        void *              arg;     ///< 在iocb调用时输出的用户自定义参数，在channel_init时指定。
        detail::nio_oper_queue_t    wrops;  ///< 发送缓存队列。
        detail::nio_oper_queue_t    rdops;  ///< 接收缓存队列。
    };

    /// init the channel.
    bool channel_init(channel_t * ch, nio::selector_t *sel, channel_io_proc cb, void *arg, err::error_t *e);

    /// create and init a new channel, return the created new channel.
    channel_t * channel_new(nio::selector_t *sel, channel_io_proc cb, void *arg, err::error_t *e);

    /// delete the channel create and returned by channel_new.
    void channel_delete(channel_t *ch);

    /// shutdown the channel, how is channel_shut_*.
    bool channel_shutdown(channel_t *ch, int how, err::error_t *e);
    
    /// open a channel asynchronously.
    bool channel_open_async(channel_t *ch, net::location_t * remote, err::error_t * err);

    /// close the channel synchronously.
    bool channel_close(channel_t *ch, err::error_t *e);

    /// close the channel asynchronously.
    bool channel_close_async(channel_t *ch, err::error_t *e);

    /// send data asynchronously with exact size.
    bool channel_sendn_async(channel_t *ch, const char * buf, int len, int64_t exp, err::error_t * err);

    /// receive any data asynchronously.
    bool channel_recvsome_async(channel_t *ch, char * buf, int len, int64_t exp, err::error_t * err);

    /// receive data with exact size asynchronously.
    bool channel_recvn_async(
        channel_t *ch, io::buffer_t *buf, int64_t exp, channel_read_callback cb, void *cbarg, err::error_t *e);


    const int listener_state_closed  = 0;    ///< nio channel state closed
    const int listener_state_open    = 2;    ///< nio channel state open
    
    const int listener_event_accepted = 1;
    const int listener_event_error    = 2;

    typedef struct nio_listener listener_t;
    typedef alg::basic_dlink_node<channel_t *> channel_ptr_node_t;
    typedef alg::basic_dlink_list<channel_t *> channel_ptr_list_t;
    
    typedef struct nio_listener_io_param
    {
        channel_t *             channel;
        const net::location_t * remote;
    } listen_io_param_t;
    typedef void (*listener_io_proc)(listener_t *lis, int event, const listen_io_param_t *io, void *arg);

    struct nio_listener {
        int                fd;
        int                state;
        selector_t  *      sel;
        listener_io_proc   iocb;
        void *             arg;
        channel_ptr_list_t chops;
    };

    bool listener_init(listener_t * lis, selector_t * sel, listener_io_proc cb, void *arg);
    bool listener_open(listener_t * lis, net::location_t * local, err::error_t *e);
    bool listener_close(listener_t * lis, err::error_t *e);
    bool listener_accept_async(listener_t * lis, channel_t * ch, err::error_t *e);

    namespace detail {
        void listener_event_callback(int sfd, int events, void *arg);
    }


    const int selectNone     = 0;
    const int selectRead     = 1;
    const int selectWrite    = 2;
    const int selectTimeout  = 4;
    const int selectError    = 8;

    class Selector {
    public:
        class Event
        {
        public:
            int    events() const;
            void * data() const;
        };
    public:
        bool add(int fd, int events, void * data, err::Error * e = nullptr);
        bool remove(int fd, err::Error * e = nullptr);
        bool set(int fd, int events, err::Error * e = nullptr);
        bool cancel(int fd, int events, err::Error *e = nullptr);
        int  wait(int ms, err::Error * e = nullptr);
        Event * revents(int i);
    }; // end class Selector

    class SocketChannel;
    class SocketListener;

    /**
     * @brief 简单的Socket服务端类。
     * 
     * 包含以下基本特性：
     *      1. 仅支持单线程多路复用I/O操作，不支持多线程，以简化逻辑提升性能。
     *      2. 支持多个Socket TCP端口或UNIX Socket监听。
     *      3. 监听器持续获取新连接，直至监听器关闭。
     *      4. 所有获取到的连接，将持续接收消息，直至连接关闭。
     */
    class SimpleSocketServer {
        class ImplClass;
        ImplClass * m_impl;

    public:
        typedef std::function<void (int sfd, int cfd, const net::Location * remote )> ListenerCallback;
        typedef std::function<void (int fd, int status, io::ConstBuffer & buffer)> SendCallback;
        typedef std::function<bool (int fd, int status, io::MutableBuffer & buffer)> RecvCallback;
        typedef std::function<void (int fd)>     CloseCallback; 
        typedef std::function<void (int status)> ServerCallback;
        typedef std::function<bool (int timer)>  TimerCallback;

        enum {
            shutdownRead  = 1,
            shutdownWrite = 2,
            shutdownBoth  = shutdownRead + shutdownWrite
        };

        enum {
            statusOk     =  0,     ///< 正常状态
            statusError  = -1,     ///< 错误状态
            statusIdle   =  0      ///< 服务空闲状态，与statusOk相同
        };

    public:
        SimpleSocketServer();
        ~SimpleSocketServer();

        int   acceptChannel(int fd, const RecvCallback & rcb, const SendCallback & scb, const CloseCallback &ccb, err::Error * e = nullptr);

        int   addListener(const net::Location &loc, const ListenerCallback & callback, err::Error * e = nullptr);

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
    enum class EnumIoType
    {
        ioSocketChannel,
        ioSocketListener
    };

    class IoBase {
    private:
        EnumIoType m_type;
    public:
        IoBase( EnumIoType type ) : m_type(type) {}

        EnumIoType type() const { return m_type; }
    }; // end class IoBase

    class SocketChannel : public IoBase {
    private:
        net::Socket  m_sock;
    public:
        SocketChannel(int fd) : IoBase(EnumIoType::ioSocketChannel), m_sock(fd) {}
        ~SocketChannel();

        int  fd() const { return m_sock.fd(); }
        bool close(err::Error * e = nullptr);
        bool shutdown(int how, err::Error *e = nullptr);
        int  shutdownFlags() const;
        int  sendSome();
        int  pushOutputBuffer(const io::ConstBuffer & buf);
        io::ConstBuffer * peekOutputBuffer();
        io::ConstBuffer * popOutputBuffer();
    }; // end class SocketChannel

    class SocketListener : public IoBase {
    private:
        net::Socket m_sock;
    public:
        SocketListener();
        ~SocketListener();
        int fd() const { return m_sock.fd(); }
        bool open(const net::Location & localAddr, err::Error * e);
        int acceptFd(net::Location * remote, err::Error * e); 
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

    public:
        Selector       m_selector;
        ListenerMap    m_listenerMap;
        ChannelMap     m_channelMap;
        int            m_idleInterval {-1};
        int            m_exitloop { false };
        ServerCallback m_serverCb;

    public:
        ImplClass()   {}
        ~ImplClass()  {}

        SocketChannel * getChannel(int fd);

        void onListenerEvent(Selector::Event * event);
        void onChannelEvent(Selector::Event * event);
        void onServerIdle();

    private:
        void onChannelWritable(ChannelEntry & entry);
        void onChannelReadable(ChannelEntry & entry);
        void onChannelError(ChannelEntry & entry);
    }; // end classs SimpleSocketServer::ImplClass

} // end namespace nio

namespace nio
{
    inline 
    void SimpleSocketServer::ImplClass::onChannelWritable(ChannelEntry & entry)
    {
        SocketChannel * channel = entry.channel;
        ssize_t n = channel->sendSome();
        io::ConstBuffer * buf = channel->peekOutputBuffer();
        assert( buf );

        if ( n >= 0 ) {
            // 需要发送的数据发送完成, 执行回调，并将该缓存发送队列中取出
            if ( buf->size() == buf->limit() ) {
                entry.sendCb(channel->fd(), statusOk, *buf);
                channel->popOutputBuffer();
            }
        } else {
            // 发送失败, 执行失败回调。只回调输出当前buffer，其余buffer在shutdownWrite时逐个返回
            entry.sendCb(channel->fd(), statusError, *buf);
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

        if ( event->events() & selectWrite ) {
            this->onChannelWritable(itEntry->second);
        }
        if ( event->events() & selectRead ) {  
            this->onChannelReadable(itEntry->second);
        }
        if ( event->events() & selectError ) {
            this->onChannelError(itEntry->second);
        }
    }

    inline 
    void SimpleSocketServer::ImplClass::onListenerEvent(Selector::Event * event)
    {
        SocketListener * listener = (SocketListener*)event->data();
        auto itEntry = m_listenerMap.find(listener->fd());
        assert ( itEntry != m_listenerMap.end());

        if ( event->events() & selectRead ) {
            int cfd;
            net::Location remote;
            err::Error error;

            while ( cfd = listener->acceptFd(&remote, &error) ) {
                if ( cfd >= 0 ) {
                    itEntry->second.callback(listener->fd(), cfd, &remote);
                } else if ( !error ) {
                    break;  // 所有排队的连接都已获取
                } else {
                    // 获取连接失败, 执行异常回调，回调过程通常关闭该监听。
                    itEntry->second.callback(listener->fd(), -1, nullptr);
                    break;
                }
                error.clear();
            }
        } 
        if ( event->events() & selectError ) {
            itEntry->second.callback(listener->fd(), -1, nullptr);
        } // end if 
    } 

    inline 
    void SimpleSocketServer::ImplClass::onServerIdle()
    {
        if ( m_serverCb ) m_serverCb(statusIdle);
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
        bool isok = m_impl->m_selector.add(fd, selectRead, ptrChannel.get(), e);
        assert( isok );

        auto it = m_impl->m_channelMap.find(fd);
        assert ( it == m_impl->m_channelMap.end() );
        ImplClass::ChannelEntry entry { ptrChannel.release(), rcb, scb, ccb};
        m_impl->m_channelMap[fd] = entry;
        return fd;
    }

    inline 
    int SimpleSocketServer::addListener(const net::Location & localAddr, const ListenerCallback & cb, err::Error * e)
    {
        std::unique_ptr<SocketListener> ptrListener(new SocketListener());
        bool isok = ptrListener->open(localAddr, e);
        if ( !isok ) {
            return false;
        }

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
    bool SimpleSocketServer::closeChannel(int fd, err::Error * e) 
    {
        SocketChannel * channel = m_impl->getChannel(fd);
        assert(channel);
        m_impl->m_selector.remove( fd, e);   // remove fd from selector synchronized
        channel->close();     // close the channel and then release it
        delete channel;
        return true;
    }

    inline 
    void SimpleSocketServer::exitLoop() {
        m_impl->m_exitloop = true;
    }

    inline 
    void SimpleSocketServer::setIdleInterval(int ms) 
    {
        m_impl->m_idleInterval = ms;
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
            if (  e ) *e = err::Error(-1, "unknown channel id");
            return false;
        }

        bool isok = itChannelEntry->second.channel->shutdown(how, e);
        if ( !isok ) return false;
        
        // 如果该channel的read和write都shutdown，则从selector中移除，
        // 移除后的selector回调中，执行closeChannel以及CloseCallback。
        if ( itChannelEntry->second.channel->shutdownFlags() == shutdownBoth ) {
            m_impl->m_selector.remove(channel);
        }

        return true;
    }

} // end namespace nio