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

} // end namespace srpc

namespace srpc {

    inline 
    bool message_check_magic(const message_t * m) 
    {
        return ( m->header.magic == srpc_magic_word);
    }

} // end namespace srpc 