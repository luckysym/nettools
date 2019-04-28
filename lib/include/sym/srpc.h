#pragma once
#include <sym/error.h>
#include <sym/nio.h>
#include <sym/network.h>

namespace srpc {
    
    /// SRPC Magic Word
    const int16_t srpc_magic_word = 0x444F;
    
    /// SRPC Message Header
    typedef struct srpc_message_header
    {
	    int16_t  magic;         ///< 消息头magic code, 取值'OD'
	    char     version;       ///< 版本号
	    char     domain;        ///< 应用域
	    int32_t  length;        ///< 报文总长
	    int32_t  sequence;      ///< 当前报文需要，用于请求报文和响应报文的匹配
	    int32_t  ttl;           ///< 报文超时时间（毫秒）,time to live
	    int64_t  timestamp;     ///< 当前报文创建时间戳（us）
	    int16_t  body_type;     ///< 报文体类型
	    int16_t  reserved;      ///< 保留字段
	    int32_t  option;        ///< 可选项字段，由不同的应用域自行定义
    } message_header_t;

    /// SRPC Message Structure
    typedef struct srpc_message {
        srpc_message_header header;   ///< header of the message
        char                body[0];  ///< body of the message 
    } message_t;

    /// check the magic of the message.
    bool message_check_magic(const message_t * m);  

    /// get the length of the message.
    int message_get_length(const message_t * m);

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

    namespace detail {
        void SRPC_SyncConnIoCallback ( nio::channel_t * ch, int event, void *io, void *arg);
    }

    typedef struct srpc_connection connection_t;

    const int srpc_status_success = 0;

    typedef void (*srpc_receive_callback)(connection_t * cn, int status, io::buffer_t * buf, void *arg);
    typedef void (*srpc_send_callback)(connection_t * cn, int status, io::buffer_t * buf, void *arg);
    typedef void (*srpc_close_callback)(connection_t * cn, int status, void *arg);

    typedef struct srpc_connection_callback {
        srpc_receive_callback  rdcb;
        srpc_send_callback     wrcb;
        srpc_close_callback    clcb;
        void                 * arg;
    } connection_cb_t;

    typedef struct srpc_read_operation {
        connection_t         * conn;
        io::buffer_t           buf;
        int64_t                exp;
        srpc_receive_callback  cb;
        void                 * cbarg;
    } readop_t;
    
    typedef alg::basic_dlink_node<readop_t> readop_node_t;
    typedef alg::basic_dlink_list<readop_t> readop_list_t;

    struct srpc_connection {
        nio::channel_t *  channel;
        connection_cb_t   cbs;
    };

    connection_t * connection_new(nio::channel_t * ch, err::error_t * e);

    void connection_delete(connection_t * cn);

    bool receive_async(connection_t * conn, io::buffer_t * buf, int64_t exp, err::error_t *e);
    
    namespace detail {
        void on_nio_received(nio::channel_t * ch, int status, io::buffer_t * buf, void *arg);
    }
} // end namespace srpc

namespace srpc {

    inline 
    bool message_check_magic(const message_t * m) 
    {
        return ( m->header.magic == srpc_magic_word);
    }

    inline 
    bool receive_async(connection_t *cn, io::buffer_t * buf, int64_t exp, err::error_t *e)
    {
        readop_t * rdop = (readop_t *)malloc(sizeof(readop_t));
        rdop->buf = *buf;
        rdop->buf.limit = sizeof(message_header_t);
        rdop->exp = exp;
        rdop->conn = cn;
        rdop->cb = cn->cbs.rdcb;
        rdop->cbarg = cn->cbs.arg;

        return nio::channel_recvn_async(cn->channel, buf, exp, detail::on_nio_received, rdop, e);
    }

    inline 
    void connection_delete(connection_t *cn) {
        free(cn);
    }

    inline 
    connection_t * connection_new(nio::channel_t * ch, connection_cb_t * cbs, err::error_t * e)
    {
        connection_t * c = (connection_t*)malloc(sizeof(connection_t));
        c->channel = ch;
        c->cbs = *cbs;

        return c;
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


namespace srpc {
namespace detail {

    void on_nio_received(nio::channel_t * ch, int status, io::buffer_t * buf, void *arg)
    {
        readop_t * rdop = (readop_t *)malloc(sizeof(readop_t));
        assert(rdop);
        message_t * m = (message_t *)buf->data;

        if ( status == nio::nio_status_success ) {

            bool is_valid = message_check_magic(m);
            if ( is_valid ) {
                int msglen = io::byteorder_btoh(m->header.length);
                if ( msglen > buf->end - buf->begin ) {
                    // 消息总长大于当前接收长度，需要继续接收
                    if ( buf->size < msglen + buf->begin ) {
                        io::buffer_realloc(buf, msglen + buf->begin);
                    }
                    buf->limit = msglen + buf->begin;
                    bool isok = nio::channel_recvn_async(ch, buf, rdop->exp, detail::on_nio_received, rdop, nullptr);
                    assert(isok);
                } else if ( msglen == buf->end - buf->begin) {
                    // 消息总长等于接收到的长度，消息接收完成，执行回调
                    rdop->cb(rdop->conn, srpc_status_success, buf, rdop->cbarg);
                }
                else {
                    // 报文大小等于接收到的
                }
            } else {
                assert("srpc::on_nio_received, invalid message magic" == nullptr);
            }
        }  else {
            assert("srpc::on_nio_received, status not success" == nullptr);
        }
    }
}} // end namespace srpc::detail 