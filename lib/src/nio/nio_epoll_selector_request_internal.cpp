#include <sym/nio.h>

namespace nio {
namespace epoll {
    
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
    
}} // end namespace nio::epoll 