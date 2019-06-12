#pragma once
#include <sym/error.h>
#include <sym/nio.h>
#include <sym/network.h>

BEGIN_SYM_NAMESPACE

namespace srpc {
    
    /// SRPC Magic Word
    const int16_t srpc_magic_word = 0x444F;

    enum MessageBodyType
	{
        typeUnknown        = 0x00,   ///< 未知类型，一般表示报文未初始化
        typeLogonRequest   = 0x01,   ///< 连接验证请求
        typeLogonResponse  = 0x02,   ///< 连接验证响应

		TYPE_LOGOFF_REQ    = 0x03,   ///< 连接断开请求
		TYPE_LOGOFF_RES    = 0x04,   ///< 连接断开响应
		TYPE_HEARTBEAT_REQ = 0x07,   ///< 心跳帧请求
		TYPE_HEARTBEAT_RES = 0x08,   ///< 心跳帧响应

        typeServiceRequest  = 0x11,   ///< RPC服务执行请求
        typeServiceResponse = 0x12   ///< RPC服务执行响应
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

#define    RPC_MAX_SERVICE_NAME_LEN  128

    typedef struct srpc_service_header {
        int64_t reg_code;
	    int64_t session_id;
	    int64_t tenant_id;
	    int64_t task_create_time;  ///< 任务创建时间，单位us
	    int32_t task_timeout;      ///< 任务总超时时间，单位毫秒
	    int32_t result;            ///< 任务执行结果，目前仅对于rpc response有效，rpc request忽略
	    int32_t rpc_body_len;
	    char    compress;
	    char    encrypt;
	    int16_t service_name_length;
	    char    service_name[RPC_MAX_SERVICE_NAME_LEN];  /// 服务名称最长字节数
    } service_header_t;

    typedef struct srpc_data_block
    {
        int32_t length;
        char    value[0];
    } datablock_t;

    typedef struct srpc_service_request {
        message_header_t header;
        service_header_t service;
        datablock_t      data[0];
    } service_request_t;

    typedef struct srpc_service_response {
        message_header_t header;
        service_header_t service;
        datablock_t      data[0];
    } service_response_t;
    
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

END_SYM_NAMESPACE
