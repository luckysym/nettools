#include <sym/nio.h>

namespace nio 
{

int  selector_run(selector_t *sel, err::error_t *err)
{
    SYM_TRACE("[trace][nio] selector_run \n");

    // 从请求队列中取出需要处理的事件操作, 先创建一个新的链表，并将requests链表转移到新建的链表，
    // 转移后清空requests，转移过程对requests加锁，这样等于批量获取，避免长时间对requests加锁。
    
    if ( sel->reqlock ) mt::mutex_lock(sel->reqlock, nullptr);
    sel_oper_list operlst = sel->requests;
    alg::dlinklist_init(&sel->requests);
    if ( sel->reqlock ) mt::mutex_unlock(sel->reqlock, nullptr);

    // 逐个获取请求节点操作。
    auto rnode = alg::dlinklist_pop_front(&operlst);
    while ( rnode ) {
        assert(rnode->value.fd >= 0);
        if ( rnode->value.ops == select_add ) {
            bool isok = epoll::selector_add_internal(sel, &rnode->value, err);
            assert(isok);
        } else if ( rnode->value.ops == select_remove) {
            bool isok = epoll::selector_remove_internal(sel, &rnode->value, err);
            assert(isok);
        } else {
            bool isok = epoll::selector_request_internal(sel, &rnode->value, err);
            assert(isok);
        }
        free(rnode);
        rnode = alg::dlinklist_pop_front(&operlst);  // 取下一个节点。
    }

    // 等待事件，并在事件触发后执行回调（不包括超时），返回触发的事件数
    int r1 = epoll::selector_run_internal(sel, err);

    // 处理超时
    int64_t now = chrono::now();
    auto tnode = sel->timeouts.front;
    while ( tnode && tnode->value.exp < now ) {  // 第一个没超时，就假设后面的都没超时
        // 超时了，执行回调，重设epoll, 踢出队列.
        //
        sel_item_t *p = sel->items.values + tnode->value.fd;

        sel_event_t se {p->fd, select_timeout | tnode->value.ops, p->arg, p->callback };
        sel->disp(&se, sel->disparg);

        if ( tnode->value.ops & select_read ) p->events &= ~(EPOLLIN);
        else if ( tnode->value.ops & select_write ) p->events &= ~(EPOLLOUT);
        else assert(false);   // 没有其他操作类型

        struct epoll_event evt;
        evt.events = p->events;
        evt.data.fd = p->fd;
        int r = epoll_ctl(sel->epfd, EPOLL_CTL_MOD, p->fd, &evt);
        assert(r == 0);

        auto n = dlinklist_pop_front(&sel->timeouts);
        assert(n == tnode);
        tnode->value.inque = 0;
        
        tnode = sel->timeouts.front;  // c重新获取第一个节点。
    }

    return r1;
} // nio::selector_run

} // end namespace io 