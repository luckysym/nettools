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

}
