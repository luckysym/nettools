#include <sym/nio.h>

namespace nio
{
nio::selector_t *  selector_init( selector_t *sel, int options, err::error_t *err)
{
    return selector_init(sel, options, detail::selector_sync_dispatcher, nullptr, err);
} // end nio::selector_init


nio::selector_t * selector_init(selector_t * sel, int options, selector_dispatch_proc disp, void *disparg, err::error_t * err)
{
    // 成员初始化
    sel->count = 0;
    alg::dlinklist_init(&sel->requests);
    alg::dlinklist_init(&sel->timeouts);

    sel->items.values = nullptr;
    sel->items.size   = 0;

    sel->events.values = nullptr;
    sel->events.size   = 0;

    sel->reqlock = nullptr;
    sel->disp = disp;
    sel->disparg = disparg;

    sel->def_wait = -1; //  default wait time, unlimited

    // create epoll fd
    sel->epfd = epoll_create1(EPOLL_CLOEXEC);
    if ( sel->epfd == -1 ) {
        err::push_error_info(err, 128, "epoll_create1 error, %d, %s", errno, strerror(errno));
        return nullptr;
    }

    // create event fd and register into epoll
    sel->evfd = eventfd(0, EFD_CLOEXEC);
    if ( sel->evfd == -1 ) {
        err::push_error_info(err, 128, "eventfd error, %d, %s", errno, strerror(errno));
        close(sel->epfd);
        sel->epfd = -1;
        return nullptr;
    }
    struct epoll_event epevt;
    epevt.events = EPOLLIN;
    epevt.data.fd = sel->evfd;
    int r = epoll_ctl(sel->epfd, EPOLL_CTL_ADD, sel->evfd, &epevt);
    if ( r != 0 ) {
        err::push_error_info(err, 128, "epoll add eventfd error, %d, %s", errno, strerror(errno));
        close(sel->epfd);
        close(sel->evfd);
        sel->epfd = -1;
        sel->evfd = -1;
        return nullptr;
    }
    array_realloc(&sel->events, 1);  // 这个是eventfd对应的
    
    // 根据options设置判断是否需要对request队列创建访问锁
    if ( options & selopt_thread_safe ) {
        sel->reqlock = (mt::mutex_t*)malloc(sizeof(mt::mutex_t));
        assert(sel->reqlock);
        bool isok = mt::mutex_init(sel->reqlock, nullptr);
        assert(isok);
    }

    return sel;
} // end selector_init    
} // end namespace nio
