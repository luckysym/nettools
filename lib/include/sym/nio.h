#pragma once 

#include <sym/error.h>

#include <sys/epoll.h>

#include <pthread.h>

/// namespace for multi-thread 
namespace mt {
    typedef pthread_mutex_t mutex_t;

    bool mutex_init(mutex_t *m, err::error_t * err);

    bool mutex_free(mutex_t *m, err::error_t * err);

    bool mutex_lock(mutex_t * m, err::error_t * err);

    bool mutex_unlock(mutex_t * m, err::error_t * err);
} // end namespace mt

namespace nio
{
    const int select_none    = 0;
    const int select_read    = 1;
    const int select_write   = 2;
    const int select_timeout = 4;
    const int select_error   = 8;
    const int select_add     = 16;
    const int select_remove  = 23;
    const int select_persist = 128;

    const int init_list_size = 128;  ///< 列表初始化大小

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

    struct selector {
        mt::mutex_t      *reqlock;   ///< lock for request operation list(requests) accessing
        sel_oper_list     requests;  ///< request operation queue
        sel_item_array    items;     ///< registered items
        sel_expire_list   timeouts;  ///< timeout queue
        sel_expire_list   persists;  ///< persist event list
        int               count;     ///< total fd registered in epoll
    };
    struct selector_epoll {
        mt::mutex_t      *reqlock;   ///< lock for request operation list(requests) accessing
        sel_oper_list     requests;  ///< request operation queue
        sel_item_array    items;     ///< registered items
        sel_expire_list   timeouts;  ///< timeout queue
        sel_expire_list   persists;  ///< persist event list
        int               count;     ///< total fd registered in epoll

        int               epfd;     ///< epoll fd
        epoll_event_array events;   ///< receive the output events from epoll_wait
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


    namespace detail {

        bool selector_add_internal(selector_t * sel, sel_oper_t * oper);
        bool selector_remove_internal(selector_t * sel, sel_oper_t *oper);
        bool selector_request_internal(selector_t *sel, sel_oper_t *oper);
        bool selector_run_internal(selector_t *sel);

        bool selector_scan_timeout(selector_t *sel);

    } // end namespace detail
} // end namespace nio

inline 
nio::selector_t * nio::selector_init(nio::selector_t *sel, int options, err::error_t *err)
{
    int fd = epoll_create1(EPOLL_CLOEXEC);
    if ( fd == -1 ) {
        err::push_error_info(err, 128, "epoll_create1 error, %d, %s", errno, strerror(errno));
        return nullptr;
    }

    sel->epfd = fd;
    sel->count = 0;
    alg::array_alloc(&sel->items, init_list_size);
    alg::array_alloc(&sel->events, init_list_size);
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
} // end nio::selector_init

inline 
bool nio::selector_destroy(nio::selector_t *sel, err::error_t *err)
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
bool nio::selector_add(nio::selector_t * sel, int fd, selector_event_calback cb, void *arg, err::error_t *err)
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
bool nio::selector_remove(nio::selector_t * sel, int fd, err::error_t *err) 
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
bool nio::selector_request(nio::selector_epoll* sel, int fd, int events, int64_t expire, err::error_info*)
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
int  nio::selector_run(nio::selector_t *sel, err::error_t *err)
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
            bool isok = detail::selector_add_internal(sel, &rnode->value);
            assert(isok);
        } else if ( rnode->value.opevents == select_remove) {
            bool isok = detail::selector_remove_internal(sel, &rnode->value);
            assert(isok);
        } else {
            bool isok = detail::selector_request_internal(sel, &rnode->value);
            assert(isok);
        }
        free(rnode);
    }

    // 等待事件，并在事件触发后执行回调，返回触发的事件数
    int r1 = detail::selector_run_internal(sel);

    // 处理超时任务
    int r2 = detail::selector_scan_timeout(sel);

    return r1;
} // nio::selector_run

