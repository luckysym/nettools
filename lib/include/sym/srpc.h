#pragma once
#include <sym/error.h>
#include <sym/nio.h>
#include <sym/network.h>

namespace srpc {
    
    /// SRPC Magic Word
    const int16_t srpc_magic_word = 0x444F;

    enum MessageBodyType
	{
		TYPE_UNKNOWN       = 0x00,   ///< 未知类型，一般表示报文未初始化
		TYPE_LOGON_REQ     = 0x01,   ///< 连接验证请求
		TYPE_LOGON_RES     = 0x02,   ///< 连接验证响应
		TYPE_LOGOFF_REQ    = 0x03,   ///< 连接断开请求
		TYPE_LOGOFF_RES    = 0x04,   ///< 连接断开响应
		TYPE_HEARTBEAT_REQ = 0x07,   ///< 心跳帧请求
		TYPE_HEARTBEAT_RES = 0x08,   ///< 心跳帧响应
		TYPE_RPC_REQ       = 0x11,   ///< RPC执行请求
		TYPE_RPC_RES       = 0x12    ///< RPC执行响应
	};
    
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

    typedef struct srpc_logon_request {
        srpc_message_header header;
        int16_t client_length;
	    int16_t server_length;
        char  body[0];
    } logon_request_t;

    typedef struct sprc_logon_reply {
        srpc_message_header header;
        int32_t result;      ///< 登录结果状态值，参阅下文CCP_LOGONRES_RESULT_宏定义
	    int16_t window;      ///< 消息发送窗口大小
	    int16_t suspend;     ///< 服务器满载后，客户端连接需要挂起的时间秒数
	    int64_t regcode;     ///< 注册码
    } logon_reply_t;
    

    /// check the magic of the message.
    bool message_check_magic(const message_t * m);  

} // end namespace srpc

namespace srpc {

    inline 
    bool message_check_magic(const message_t * m) 
    {
        return ( m->header.magic == srpc_magic_word);
    }

} // end namespace srpc 