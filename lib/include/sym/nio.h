#pragma once 

#include <sym/error.h>

#include <sys/epoll.h>

namespace nio
{
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
} // nio::selector_run

