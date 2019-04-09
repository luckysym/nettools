#pragma once 

#include <sym/error.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>

/// namespace for multi-thread 
namespace mt {
    typedef pthread_mutex_t mutex_t;

    bool mutex_init(mutex_t *m, err::error_t * err);

    bool mutex_free(mutex_t *m, err::error_t * err);

    bool mutex_lock(mutex_t * m, err::error_t * err);

    bool mutex_unlock(mutex_t * m, err::error_t * err);
} // end namespace mt

namespace mt {

inline 
bool mutex_init(mutex_t *m, err::error_t *e)
{
    int r = pthread_mutex_init(m, nullptr);
    if ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_init error, %d, %s", strerror(r));
        }
        return false;
    } 
}

inline 
bool mutex_free(mutex_t *m, err::error_t *e) {
    int r = pthread_mutex_destroy(m);
    if ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_destroy error, %d, %s", strerror(r));
        }
        return false;
    }
}

inline 
bool mutex_lock(mutex_t *m, err::error_t *e ) {
    int r = pthread_mutex_lock(m);
    if  ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_lock error, %d, %s", strerror(r));
        }
        return false;
    }
}

inline 
bool mutex_unlock(mutex_t *m, err::error_t *e ) {
    int r = pthread_mutex_unlock(m);
    if  ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_unlock error, %d, %s", strerror(r));
        }
        return false;
    }
}

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

    const int init_list_size = 128;  ///< 列表初始化大小
    const int selopt_thread_safe = 1;  ///< selector support multi-thread

    /// selector event callback function type
    typedef void (*selector_event_calback)(int fd, int event, void *arg);

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
        selector_event_calback  callback;
        void   *arg;
        sel_expire_node rd;
        sel_expire_node wr; 
    } sel_item_t;
    typedef alg::basic_dlink_node<sel_item_t> sel_item_node;

    typedef struct select_operation {
        int     fd;
        int     ops;  ///< requested operation events
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
        int               count;     ///< total fd registered in epoll
    };
    struct selector_epoll {
        mt::mutex_t      *reqlock;   ///< lock for request operation list(requests) accessing
        sel_oper_list     requests;  ///< request operation queue
        sel_item_array    items;     ///< registered items
        sel_expire_list   timeouts;  ///< timeout queue
        int               count;     ///< total fd registered in epoll, event fd not involved

        int               evfd;     ///< event fd for nofitier
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

    namespace epoll {

        sel_item_t * selector_add_internal(selector_epoll * sel, sel_oper_t * oper, err::error_t *e);
        bool selector_remove_internal(selector_epoll * sel, sel_oper_t *oper, err::error_t *e);
        bool selector_request_internal(selector_epoll *sel, sel_oper_t *oper, err::error_t *e);
        bool selector_run_internal(selector_epoll *sel, err::error_t *e);

    } // end namespace detail
} // end namespace nio

inline 
nio::selector_t * nio::selector_init(nio::selector_t *sel, int options, err::error_t *err)
{
    // 成员初始化
    sel->count = 0;
    alg::dlinklist_init(&sel->requests);
    alg::dlinklist_init(&sel->timeouts);

    sel->items.values = nullptr;
    sel->items.size   = 0;

    sel->events.values = nullptr;
    sel->events.size   = 0;

    sel->reqlock = nullptr;

    // create epoll fd
    sel->epfd = epoll_create1(EPOLL_CLOEXEC);
    if ( sel->epfd == -1 ) {
        err::push_error_info(err, 128, "epoll_create1 error, %d, %s", errno, strerror(errno));
        return nullptr;
    }

    // create event fd and register into epoll
    sel->evfd = eventfd(0, EFD_CLOEXEC);
    if ( sel->evfd == -1 ) {
        err::push_error_info(err, 128, "eventfd error, %d, %s", errno, strerror(errno));
        close(sel->epfd);
        sel->epfd = -1;
        return nullptr;
    }
    struct epoll_event epevt;
    epevt.events = EPOLLIN;
    epevt.data.fd = sel->evfd;
    int r = epoll_ctl(sel->epfd, EPOLL_CTL_ADD, sel->evfd, &epevt);
    if ( r != 0 ) {
        err::push_error_info(err, 128, "epoll add eventfd error, %d, %s", errno, strerror(errno));
        close(sel->epfd);
        close(sel->evfd);
        sel->epfd = -1;
        sel->evfd = -1;
        return nullptr;
    }
    array_realloc(&sel->events, 1);  // 这个是eventfd对应的
    
    // 根据options设置判断是否需要对request队列创建访问锁
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

    // close epoll fd and event fd
    close(sel->epfd);
    close(sel->evfd);
    sel->epfd = -1;
    sel->evfd = -1;
    
    return true;
} 

inline 
bool nio::selector_add(nio::selector_t * sel, int fd, selector_event_calback cb, void *arg, err::error_t *err)
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.ops = select_add;
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

    int64_t n = 1;
    int r = write(sel->evfd, &n, sizeof(n));   // 通知event fd
    assert( r > 0);

    SYM_TRACE_VA("[trace][nio] selector_add, fd: %d\n", fd);

    return true;
}

inline 
bool nio::selector_remove(nio::selector_t * sel, int fd, err::error_t *err) 
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.ops = select_remove;
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

    int64_t n = 1;
    int r = write(sel->evfd, &n, sizeof(n));   // 通知event fd
    assert( r > 0);

    SYM_TRACE_VA("[trace][nio] selector_remove, fd: %d\n", fd);

    return true;
}

inline 
bool nio::selector_request(nio::selector_epoll* sel, int fd, int events, int64_t expire, err::error_info*)
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.ops = events;
    node->value.expire = expire==-1?INT64_MAX:expire;
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

    int64_t n = 1;
    int r = write(sel->evfd, &n, sizeof(n));   // 通知event fd
    assert( r > 0);


    SYM_TRACE_VA("[trace][nio] selector_request, fd: %d, ops: %d\n", fd, events);

    return true;
}

inline 
int  nio::selector_run(nio::selector_t *sel, err::error_t *err)
{
    SYM_TRACE("[trace][nio] selector_run \n");

    // 从请求队列中取出需要处理的事件操作, 先创建一个新的链表，并将requests链表转移到新建的链表，
    // 转移后清空requests，转移过程对requests加锁，这样等于批量获取，避免长时间对requests加锁。
    
    if ( sel->reqlock ) mt::mutex_lock(sel->reqlock, nullptr);
    sel_oper_list operlst = sel->requests;
    alg::dlinklist_init(&sel->requests);
    if ( sel->reqlock ) mt::mutex_unlock(sel->reqlock, nullptr);

    // 逐个获取请求节点操作。
    auto rnode = alg::dlinklist_pop_front(&operlst);
    while ( rnode ) {
        assert(rnode->value.fd >= 0);
        if ( rnode->value.ops == select_add ) {
            bool isok = epoll::selector_add_internal(sel, &rnode->value, err);
            assert(isok);
        } else if ( rnode->value.ops == select_remove) {
            bool isok = epoll::selector_remove_internal(sel, &rnode->value, err);
            assert(isok);
        } else {
            bool isok = epoll::selector_request_internal(sel, &rnode->value, err);
            assert(isok);
        }
        free(rnode);
        rnode = alg::dlinklist_pop_front(&operlst);  // 取下一个节点。
    }

    // 等待事件，并在事件触发后执行回调（不包括超时），返回触发的事件数
    int r1 = epoll::selector_run_internal(sel, err);

    // 处理超时
    int64_t now = chrono::now();
    auto tnode = sel->timeouts.front;
    while ( tnode && tnode->value.exp < now ) {  // 第一个没超时，就假设后面的都没超时
        // 超时了，执行回调，重设epoll, 踢出队列.
        //
        sel_item_t *p = sel->items.values + tnode->value.fd;
        p->callback(p->fd, select_timeout | tnode->value.ops, p->arg);

        if ( tnode->value.ops & select_read ) p->events &= ~(EPOLLIN);
        else if ( tnode->value.ops & select_write ) p->events &= ~(EPOLLOUT);
        else assert(false);   // 没有其他操作类型

        struct epoll_event evt;
        evt.events = p->events;
        evt.data.fd = p->fd;
        int r = epoll_ctl(sel->epfd, EPOLL_CTL_MOD, p->fd, &evt);
        assert(r == 0);

        auto n = dlinklist_pop_front(&sel->timeouts);
        assert(n == tnode);
        tnode->value.inque = 0;
        
        tnode = sel->timeouts.front;  // c重新获取第一个节点。
    }

    return r1;
} // nio::selector_run

namespace nio {
namespace epoll {

sel_item_t * selector_add_internal(selector_epoll *sel, sel_oper_t *oper, err::error_t *err)
{
    // 扩充表
    if ( oper->fd >= sel->items.size ) {
        select_item item;
        item.fd = -1;
        item.rd.value.ops = select_read;
        item.wr.value.ops = select_write;
        alg::array_realloc(&sel->items, oper->fd + 1, &item);
    }

    // get select_item pointer from table
    select_item * p = sel->items.values + oper->fd;
    assert(p->fd == -1);
    
    // registry to epoll
    struct epoll_event e;
    e.events = 0;
    e.data.fd = oper->fd;
    int r = epoll_ctl(sel->epfd, EPOLL_CTL_ADD, oper->fd, &e);
    assert(r == 0);

    // add to table
    p->fd = p->wr.value.fd = p->rd.value.fd = oper->fd;
    p->events = oper->ops;
    p->callback = oper->callback;
    p->arg = oper->arg;

    // added callback;
    p->callback(p->fd, select_add, p->arg);

    // extend epoll events 
    sel->count += 1;
    array_realloc(&sel->events, sel->count + 1);  // 还有一个是event fd
    
    SYM_TRACE_VA("[trace][nio] selector_add_internal, fd:%d\n", oper->fd);

    return p;
}

bool selector_remove_internal(selector_epoll * sel, sel_oper_t *oper, err::error_t *err)
{
    // get select_item pointer from table
    select_item * p = sel->items.values + oper->fd;
    assert( p->fd == oper->fd );

    // remove from epoll
    int r = epoll_ctl(sel->epfd, EPOLL_CTL_DEL, oper->fd, nullptr);
    assert(r == 0);

    // removal callback
    p->callback(p->fd, select_remove, p->arg);

    // remove from timeout queue
    auto node = sel->timeouts.front;
    while ( node && p->rd.value.inque && p->wr.value.inque) {
        auto node2 = node->next;
        if ( node->value.fd == p->fd ) {
            auto n = dlinklist_remove(&sel->timeouts, node);
            node->value.inque = 0;
        }
        node = node2;
    }

    // remove from table;
    p->fd = -1;
    p->events = 0;
    p->callback = nullptr;
    p->arg = nullptr;

    // reduce epoll result events
    sel->count -= 1;
    array_realloc(&sel->events, sel->count + 1); // 还有一个event fd

    SYM_TRACE_VA("[trace][nio] selector_remove_internal, fd:%d, ops: %d\n", oper->fd, oper->ops);
    return true;
}

inline 
bool selector_request_internal(selector_epoll *sel, sel_oper_t *oper, err::error_t *e)
{
    // get select_item pointer from table
    select_item * p = sel->items.values + oper->fd;
    assert( p->fd == oper->fd );

    // ops转成epoll events，并将对应rd/wr节点放入timeout队列, 
    // 如果当前的请求的操作已经设置，则重新设置。同一事件在一次run过程中只能存在一个示例。
    int events = 0;
    if ( oper->ops & select_read ) {
        events |= EPOLLIN;

        // timeout节点放入timeouts队列，如果已经在队列里，取出后放入
        if ( p->rd.value.inque ) alg::dlinklist_remove(&sel->timeouts, &p->rd);
        p->rd.value.exp = oper->expire;
        alg::dlinklist_push_back(&sel->timeouts, &p->rd);
    }
    if ( oper->ops & select_write) {
        events |= EPOLLOUT; 
        // timeout节点放入timeouts队列，如果已经在队列里，取出后放入
        if ( p->wr.value.inque ) alg::dlinklist_remove(&sel->timeouts, &p->wr);
        p->wr.value.exp = oper->expire;
        alg::dlinklist_push_back(&sel->timeouts, &p->wr);
    }
    p->events |= events;  // 请求的events

    // modify epoll
    struct epoll_event ev;
    ev.events = p->events;
    ev.data.fd = p->fd;
    int r = epoll_ctl(sel->epfd, EPOLL_CTL_MOD, p->fd, &ev);
    assert(r == 0);

    SYM_TRACE_VA("[trace][nio] selector_request_internal, fd:%d, ops: %d\n", oper->fd, oper->ops);

    return true;
}

inline 
bool selector_run_internal(selector_epoll *sel, err::error_t *e)
{
    // if ( sel->timeouts.size == 0 ) return 0;  // 没有需要等待的事件

    // 计算超时时间。从超时队列获取第一个节点。如果第一个节点已经超时，超时时间设为0.

    int timeout = -1;
    if ( sel->timeouts.front ) {
        int64_t now = chrono::now();
        auto tn = sel->timeouts.front;
        if ( tn->value.exp != INT64_MAX && tn->value.exp > now ) 
            timeout = (int)((tn->value.exp - now) / 1000);
        SYM_TRACE_VA("[trace][nio] begin epoll_wait, timeout %d, exp: %lld now %lld\n", 
            timeout, tn->value.exp, now);
    } else {
        SYM_TRACE_VA("[trace][nio] begin epoll_wait, no timeout %d\n", timeout);
    }
    
    // epoll wait
    int r = epoll_wait(sel->epfd, sel->events.values, sel->events.size, timeout);
    if ( r >= 0 ) {
        for( int i = 0; i < r; ++i ) {
            epoll_event * evt = sel->events.values + i;
            int fd = evt->data.fd;

            if ( fd == sel->evfd ) {
                SYM_TRACE_VA("[trace][nio] epoll_wait, event fd notified, fd: %d\n", fd);
                if ( evt->events == EPOLLIN) {
                    int64_t n;
                    int r = read(sel->evfd, &n, sizeof(n));
                    assert(r > 0 );
                } else {
                    assert(false);
                }
                continue;
            }

            int events = evt->events;
            sel_item_t * p = sel->items.values + fd;

            // 无论是否异常，有消息可读就先读. 而异常和可写，只处理一个
            if ( events & EPOLLIN ) {
                p->callback(fd, select_read, p->arg);  // 回调
                dlinklist_remove(&sel->timeouts, &p->rd);
                p->rd.value.inque = 0;
                p->events &= (~EPOLLIN);
            }
            if ( events & EPOLLERR ) {
                p->callback(fd, select_error, p->arg);
                if ( p->rd.value.inque ) {
                    dlinklist_remove(&sel->timeouts, &p->rd);
                    p->rd.value.inque = 0;
                }
                if ( p->wr.value.inque ) {
                    dlinklist_remove(&sel->timeouts, &p->wr);
                    p->wr.value.inque = 0;
                }
                p->events = 0;
            } else if ( events & EPOLLOUT ) {
                p->callback(fd, select_write, p->arg);
                dlinklist_remove(&sel->timeouts, &p->wr);
                p->wr.value.inque = 0;
                p->events &= (~EPOLLOUT);
            }

            // 相关事件已完成，重新设置并去掉已发生的epoll监听
            struct epoll_event evt1;
            evt1.events = p->events;
            evt1.data.fd = fd;
            int c = epoll_ctl(sel->epfd, EPOLL_CTL_MOD, fd, &evt1);
            assert(c == 0);
        }
    } else {
        if ( errno != EINTR )  assert("epoll_wait error" == nullptr);
    }
    return r;
}

}} // end namespace nio::detail