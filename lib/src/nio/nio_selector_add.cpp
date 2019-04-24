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

} // end namespace nio 