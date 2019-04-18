#include <sym/nio.h>

namespace nio 
{
    
} // end namespace nio 

namespace nio 
{
    
bool selector_destroy( selector_t *sel, err::error_t *err)
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

    // close epoll fd and event fd
    close(sel->epfd);
    close(sel->evfd);
    sel->epfd = -1;
    sel->evfd = -1;
    
    return true;
} 

} // end namespace nio 
