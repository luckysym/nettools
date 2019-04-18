#include <sym/nio.h>

namespace nio 
{
namespace epoll 
{
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
    sel_event_t se {p->fd, select_add, p->arg, p->callback }; 
    sel->disp(&se, sel->disparg);
    
    // extend epoll events 
    sel->count += 1;
    array_realloc(&sel->events, sel->count + 1);  // 还有一个是event fd
    
    SYM_TRACE_VA("[trace][nio] selector_add_internal, fd:%d\n", oper->fd);

    return p;
}
} // end namespace epoll 
} // end namespace nio