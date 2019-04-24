#include <sym/nio.h>

namespace nio {
namespace epoll {

bool selector_remove_internal(selector_epoll * sel, sel_oper_t *oper, err::error_t *err)
{
    // get select_item pointer from table
    select_item * p = sel->items.values + oper->fd;
    assert( p->fd == oper->fd );

    // remove from epoll
    int r = epoll_ctl(sel->epfd, EPOLL_CTL_DEL, oper->fd, nullptr);
    assert(r == 0);

    // removal callback if the operation is asynchronous.
    if ( oper->async ) {
        sel_event_t se {p->fd, select_remove, p->arg, p->callback }; 
        sel->disp(&se, sel->disparg);
    }
    
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
    
}} // end namespace nio::epoll 