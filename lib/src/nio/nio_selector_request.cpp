#include <sym/nio.h>

namespace nio {

bool selector_request(selector_t* sel, int fd, int events, int64_t expire, err::error_info*)
{
    sel_oper_node * node = (sel_oper_node *)malloc(sizeof(sel_oper_node));
    node->value.fd = fd;
    node->value.ops = events;
    node->value.expire = expire==-1?INT64_MAX:expire;
    node->value.callback = nullptr;
    node->value.arg = nullptr;
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


    SYM_TRACE_VA("[trace][nio] selector_request, fd: %d, ops: %d\n", fd, events);

    return true;
}


} // end namespace nio 
