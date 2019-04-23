#pragma once
#include <sym/error.h>
#include <sym/nio.h>
#include <sym/network.h>

namespace srpc {
    
    class SRPC_SyncConnection {
    private:
        nio::selector_t  m_sel;
        nio::channel_t   m_ch;
        err::error_t     m_err;

    public:
        SRPC_SyncConnection();
        ~SRPC_SyncConnection();
        bool open(const char * url);
        bool close();

        const err::error_t * last_error() const { return &m_err;}
    }; // end class SRPC_SyncConnection

    typedef alg::basic_dlink_node<nio::listener_t> listener_node_t;
    typedef alg::basic_dlink_node<nio::channel_t>  channel_node_t;

    typedef alg::basic_dlink_list<nio::listener_t> listener_list_t;
    typedef alg::basic_dlink_list<nio::channel_t>  channel_list_t;

    typedef struct srpc_async_server async_server_t;
    
    const int srpc_event_channel_accepted = 1;

    typedef struct srpc_channel_param {
        nio::channel_t * channel;
    } channel_param_t;

    typedef void (*rpc_async_server_callback)(
        async_server_t *as, int event, void * param, void *arg);

    struct srpc_async_server {
        nio::selector_t  selector;
        listener_list_t  listeners;
        channel_list_t   channels;
        rpc_async_server_callback  cb;
        void *                     cbarg;
    };

    bool async_server_init(async_server_t *as, err::error_t *e);

    nio::listener_t * async_server_add_listener(async_server_t *as, const char *local, err::error_t *e);

    int  async_server_run(async_server_t *as, err::error_t *e);

    bool async_server_close(async_server_t *as, err::error_t *e);

    namespace detail {

        void SRPC_SyncConnIoCallback ( nio::channel_t * ch, int event, void *io, void *arg);
        void srpc_async_listener_callback(nio::listener_t *lis, int event, nio::listen_io_param_t * io, void *arg);
        void srpc_async_channel_callback(nio::channel_t *ch, int event, void *io, void *arg);
    }
    
    class SRPC_AsyncServer {
    private:
        nio::selector_t  m_sel;
        listener_list_t  m_lss;
        channel_list_t   m_chs;
        err::error_t     m_err;

    public:
        bool add_listener(const char * url);
        bool run();
    }; // end class SRPC_AsyncServer

    namespace detail {
        
    } // end namespace detail
} // end namespace srpc

namespace srpc {

    inline 
    bool async_server_init(async_server_t * as, err::error_t *e) 
    {
        bool isok;
        isok = nio::selector_init(&as->selector, 0, e);
        if ( !isok ) return false;

        isok = alg::dlinklist_init(&as->listeners);
        assert(isok);

        isok = alg::dlinklist_init(&as->channels);
        assert(isok);
        return isok;
    }

    inline 
    nio::listener_t * 
    async_server_add_listener(async_server_t *as, const char *local, err::error_t *e)
    {
        net::location_t clocal;
        net::location_init(&clocal);
        bool isok = net::location_from_url(&clocal, local);
        if ( !isok ) {
            if ( e ) err::push_error_info(e, 128, "bad url: %s", local);
            return nullptr;            
        }

        listener_node_t * lsn = (listener_node_t*)malloc(sizeof(listener_node_t));
        nio::listener_t * pr = &lsn->value;

        isok = nio::listener_init(&lsn->value, &as->selector, detail::srpc_async_listener_callback, as);
        assert(isok);

        isok = nio::listener_open(&lsn->value, &clocal, e);
        if ( !isok ) {
            free(lsn);
            pr = nullptr;
        } else {
            alg::dlinklist_push_back(&as->listeners, lsn);
        }

        // 异步接收连接
        channel_node_t *cn = (channel_node_t *)malloc(sizeof(channel_node_t));
        isok = nio::channel_init(&cn->value, &as->selector, detail::srpc_async_channel_callback, as, e);
        assert(isok);
        isok = nio::listener_accept_async(&lsn->value, &cn->value, e);
        assert(isok);

        net::location_free(&clocal);
        return pr;
    }

    inline 
    int async_server_run(async_server_t *as, err::error_t *e)
    {
        int r = nio::selector_run(&as->selector, e);
    }

    inline 
    bool async_server_close(async_server_t *as, err::error_t *e)
    {
        bool isok;

        // close listener
        listener_node_t * ln = nullptr;
        while ( ln = alg::dlinklist_pop_front(&as->listeners) ) {
            err::error_t e1;
            err::init_error_info(&e1);
            isok = nio::listener_close(&ln->value, &e1);
            assert(isok);
            free(ln);
            err::free_error_info(&e1);
        }

        // close channel
        channel_node_t * cn = nullptr;
        while ( cn == alg::dlinklist_pop_front(&as->channels) ) {
            err::error_t e2;
            err::init_error_info(&e2);
            isok = nio::channel_close(&cn->value, &e2);
            assert(isok);
            free(ln);
            err::free_error_info(&e2);
        }
        return true;
    } // end async_server_close

    inline 
    void detail::srpc_async_listener_callback(nio::listener_t *lis, int event, nio::listen_io_param_t * io, void *arg)
    {
        async_server_t * as = (async_server_t *)arg;
        assert(as);

        err::error_t err;
        err::init_error_info(&err);

        if ( event == nio::listener_event_accepted ) {
            // channel在async_server_add_listener时创建
            channel_node_t * cn = (channel_node_t *)io->channel;
            alg::dlinklist_push_back(&as->channels, cn);  // 放入列表
            
            // do callback
            channel_param_t param;
            param.channel = &cn->value;
            as->cb(as, srpc_event_channel_accepted, &param, as->cbarg);
        } else {
            assert("srpc_async_listener_callback unknown event" == nullptr);
        }
    } // srpc_async_listener_callback

    inline
    void detail::srpc_async_channel_callback(nio::channel_t *ch, int event, void *io, void *arg)
    {
        assert("srpc_async_channel_callback" == nullptr);
    }

    inline
    SRPC_SyncConnection::SRPC_SyncConnection()
    { 
        err::init_error_info(&m_err);

        bool isok = nio::selector_init(&m_sel, 0, &m_err);
        assert(isok);

        auto p = nio::channel_init(&m_ch, &m_sel, detail::SRPC_SyncConnIoCallback, this, &m_err);
        assert(p);
    }

    inline
    SRPC_SyncConnection::~SRPC_SyncConnection()
    {
        nio::selector_destroy(&m_sel, nullptr);
        err::free_error_info(&m_err);
    }

    inline
    bool SRPC_SyncConnection::open(const char * url) {
        net::location_t remote;
        net::location_init(&remote);
        auto p = net::location_from_url(&remote, url);
        assert(p);

        err::free_error_info(&m_err);
        bool isok = channel_open_async(&m_ch, &remote, &m_err);
        if ( !isok ) return isok;    // open failed

        m_sel.def_wait = 0;
        while (1) {
            err::free_error_info(&m_err);
            int r = nio::selector_run(&m_sel, &m_err);
            if ( r < 0 ) break;
            if ( m_ch.state != nio::channel_state_opening ) break;

        }

        if ( m_ch.state == nio::channel_state_open ) return true;
        else {
            // 连接失败，关闭channel。
            err::free_error_info(&m_err);
            nio::channel_close(&m_ch, &m_err);
            return false;
        }
    }

    inline
    bool SRPC_SyncConnection::close() {
        if ( m_ch.state != nio::channel_state_closed ) {
            err::free_error_info(&m_err);
            nio::channel_close_async(&m_ch, &m_err);
            
            while ( m_ch.state != nio::channel_state_closed ) {
                err::free_error_info(&m_err);
                int r = nio::selector_run (&m_sel, &m_err);
                if ( r < 0 ) break;
            }
        }
        return true;
    }

    inline
    void detail::SRPC_SyncConnIoCallback(nio::channel_t * ch, int event, void *io, void *arg)
    {
        SRPC_SyncConnection * connect = (SRPC_SyncConnection*)arg;
        if ( event == nio::channel_event_connected ) {
            
        } 
        if ( event == nio::channel_event_error | nio::channel_event_connected) {
            SYM_TRACE("connection failed");
        }
        else {
            assert("SRPC_SyncConnIoCallback" == nullptr);
        }
    }
} // end namespace srpc
