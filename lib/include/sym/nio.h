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
        typedef std::function<void (int fd)> CloseCallback; 
        typedef std::function<void (int status)> ServerCallback;
        typedef std::function<void (int timer)> TimerCallback;
    
    public:
        SimpleSocketServer();

        int  addListener(const net::Location &loc, const ListenerCallback & callback, err::Error * error);
        int  addTimer(int interval, const TimerCallback & cb, err::Error * e);
        
        int  closeListener(int fd);
        int  closeChannel(int fd);
        int  closeTimer(int timer);

        int  send(int channel, const io::ConstBuffer & buffer, err::Error * e);
        int  send(int channel, const io::ConstBuffer & buffer, int64_t expire, err::Error * e);
        
        void setServerCallback(ServerCallback & callback);
        bool acceptChannel(int fd, const RecvCallback & rcb, const SendCallback & scb, const CloseCallback &ccb, err::Error * e);

        bool run(err::Error * e);
        void exitLoop();
        bool wakeup();
    }; // end class SimpleSocketServer
} // end namespace nio



