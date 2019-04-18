#include <sym/nio.h>

namespace nio {
namespace epoll {
    
bool selector_run_internal(selector_epoll *sel, err::error_t *e)
{
    // if ( sel->timeouts.size == 0 ) return 0;  // 没有需要等待的事件

    // 计算超时时间。从超时队列获取第一个节点。如果第一个节点已经超时，超时时间设为0.

    int timeout = sel->def_wait;
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
                sel_event_t se {fd, select_read, p->arg, p->callback};
                sel->disp(&se, sel->disparg);
                dlinklist_remove(&sel->timeouts, &p->rd);
                p->rd.value.inque = 0;
                p->events &= (~EPOLLIN);
            }
            if ( events & EPOLLERR ) {
                sel_event_t se {fd, select_error, p->arg, p->callback};
                sel->disp(&se, sel->disparg);
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
                sel_event_t se {fd, select_write, p->arg, p->callback};
                sel->disp(&se, sel->disparg);
                // p->callback(fd, select_write, p->arg);
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
}} // end namespace nio::epoll 