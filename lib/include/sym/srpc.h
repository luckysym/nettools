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
        void SRPC_SyncConnIoCallback ( nio::channel_t * ch, int event, void *io, void *arg);
    
        void SRC_AsyncListenIoCallback(nio::listener_t *lis, nio::channel_t * ch, void *arg);
    } // end namespace detail
} // end namespace srpc

namespace srpc {

    inline 
    bool SRPC_AsyncServer::add_listener(const char * url) 
    {
        err::init_error_info(&m_err);
        listener_node_t * lsn = (listener_node_t*)malloc(sizeof(listener_node_t));
        
        bool isok = nio::listener_init(&lsn->value, &m_sel, detail::SRC_AsyncListenIoCallback, lsn);
        assert(isok);

        net::location_t local;
        net::location_init(&local);
        auto p = net::location_from_url(&local, url);
        assert(p);

        isok = nio::listener_open(&lsn->value, &local, &m_err);
        assert(isok);

        net::location_free(&local);
        alg::dlinklist_push_back(&m_lss, lsn);

        return isok;
    }

    inline 
    bool SRPC_AsyncServer::run() 
    {
        err::init_error_info(&m_err);
        while (1) {
            int r = nio::selector_run(&m_sel, &m_err);
            assert( r >= 0 );
        }

        return true;
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
