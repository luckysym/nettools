#include <sym/nio.h>

namespace nio 
{
bool selector_remove(selector_t * sel, int fd, err::error_t *err) 
{
    sel_oper_t oper;
    oper.fd = fd;
    oper.ops = select_remove;
    oper.async = false;   // not a async operation
    
    SYM_TRACE_VA("[trace][nio] selector_remove, fd: %d\n", fd);

    return epoll::selector_remove_internal(sel, &oper, err);
}


bool selector_remove_async( selector_t * sel, int fd)
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.ops = select_remove;
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
    int r = write(sel->evfd, &n, sizeof(n));
    assert( r > 0);

    SYM_TRACE_VA("[trace][nio] selector_add_async, fd: %d\n", fd);
    return true;
}

} // end namespace 
