#include <sym/nio.h>

namespace nio 
{

bool selector_add( selector_t * sel, int fd, selector_event_proc cb, void *arg, err::error_t *err)
{
    sel_oper_t oper;
    oper.fd = fd;
    oper.ops = select_add;
    oper.callback = cb;
    oper.arg = arg;
    oper.async = false;
    
    sel_item_t * item = epoll::selector_add_internal(sel, &oper, err);
    SYM_TRACE_VA("[trace][nio] selector_add, fd: %d\n", fd);

    return item != nullptr;
}


bool selector_add_async( selector_t * sel, int fd, selector_event_proc cb, void *arg)
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.ops = select_add;
    node->value.expire = INT64_MAX;
    node->value.callback = cb;
    node->value.arg = arg;
    node->value.async = true;
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
    int r = write(sel->evfd, &n, sizeof(n));   // é€šçŸ¥event fd
    assert( r > 0);

    SYM_TRACE_VA("[trace][nio] selector_add_async, fd: %d\n", fd);
    return true;
}

} // end namespace nio 