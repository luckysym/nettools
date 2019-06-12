#pragma once 

#include <memory.h>
#include <vector>
#include <functional>
#include <sym/error.h>

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE 

namespace redis
{
    class Value 
    {
    public:
        enum {
            vtNull,
            vtStatus,
            vtString,
            vtInteger,
            vtList
        };

        using ValueList = std::vector<Value>;
    private:
        int         m_type { vtNull };
        bool        m_status;
        std::string m_str;
        int64_t     m_nval;
        ValueList   m_list;
        
    public:
        bool status() const { return m_status; }

        void setStatus(bool status, const char *msg, int len) 
        {
            m_type =  vtStatus ;
            m_status = status;
            m_str.assign(msg, len); 
        }

        int  type() const { return m_type; }

        void setType(int type) { m_type = type; } 

        void setNull() { m_type == vtNull; }

        void setString(const char * data, int len) {
            m_type = vtString;
            m_str.assign(data, len);
        }

        void setInt(int64_t val) { 
            m_type = vtInteger;
            m_nval = val; 
        }

        std::string getString() const { return m_str; }

        int64_t getInt() const { return m_nval; }

        ValueList & list() { return m_list; }

        void reset() {
            m_type = vtNull;
            m_str.clear();
            m_status = true;
        }
    }; // end class Value

    using SendHandler = std::function<int (const char * cmd, int len, int timeout, err::Error * e)>;
    using RecvHandler = std::function<int (char * buf, int len, int timeout, err::Error *e)>;

    class ValueDecoder
    {
    private:
        static const int stateInit   = 0;
        static const int stateFinish = 1;
        static const int stateArray  = 2;

        struct ValueItem {
            int type;
        };

    private:
        RecvHandler            m_rh;
        std::vector<char>      m_buf;
        int                    m_len;   // m_buf中收到的有效数据总长
        int                    m_pos;   // m_buf中当前解析位置

    public:
        ValueDecoder(RecvHandler rh) : m_rh(rh) {}

        int  parse(Value * value, int timeout, err::Error *e = nullptr);
        int  receiveData(int timeout, err::Error *e = nullptr);

    private:
        int parseStatus(Value * value, int timeout, err::Error * e = nullptr);
        int parseInteger(Value * value, int timeout, err::Error * e = nullptr);
        int parseBulkStrings(Value * value, int timeout, err::Error * e = nullptr);
        int parseArray(Value * value, int timeout, err::Error * e = nullptr);
    }; // end class ValueDecoder

    class Command 
    {
    private:
        std::vector<char> m_buf;
        std::vector<std::string> m_args;
        SendHandler       m_sh;
        RecvHandler       m_rh;
        int               m_timeout;

    public:
        Command(SendHandler sh, RecvHandler rh) : m_sh(sh), m_rh(rh) {}

        Command & operator<<(const char * str) { return this->append(str); }

        Command & assign(const char * str);
        Command & append(const char * str);
        
        bool execute(Value * value, err::Error *e = nullptr);
        bool execute(Value * value, const char * text, err::Error *e = nullptr);
        
        void setTimeout(int timeout) { m_timeout = timeout; };

        /// 重置当前命令，删除之前的命令缓存
        void reset() { m_buf.resize(0); m_args.resize(0); }
    }; // end class Command

    Command & Command::assign(const char * str) 
    {
        m_buf.resize(0);
        m_args.resize(0);

        int len1 = str?strlen(str):0;
        if( m_args.capacity() == m_args.size() ) m_args.reserve(m_args.capacity() + 8);
        m_args.push_back(std::string(str, len1));
        return *this;
    }

    Command & Command::append(const char * str) {
        int len1 = str?strlen(str):0;
        if( m_args.capacity() == m_args.size() ) m_args.reserve(m_args.capacity() + 8);
        m_args.push_back(std::string(str, len1));
        return *this;
    }

    bool Command::execute(Value * value, const char * text, err::Error * e) 
    {
        if ( text ) {
            int len = strlen(text);
            if ( len >= 2 && text[len - 1] == '\n' && text[len - 2] == '\r') {
                m_buf.resize( len );
                memcpy(&m_buf[0], text, len);
            } else {
                m_buf.resize(len + 2);
                memcpy(&m_buf[0], text, len);
                m_buf[len] = '\r';
                m_buf[len + 1] = '\n';
            }
        } else {
            m_buf.resize(0);
        }

        return this->execute(value, e);
    }

    bool Command::execute(Value * value, err::Error * e ) 
    {
        if ( m_buf.empty() ) {
            int total_len = 0;
            for ( auto it = m_args.begin(); it != m_args.end(); ++it) {
                total_len +=it->length();
            }
            total_len += m_args.size() * 32;
            m_buf.resize(total_len);

            int pos = 0;
            int remain = total_len;
            int r = snprintf(&m_buf[pos], remain, "*%d\r\n", (int)m_args.size());
            pos += r; remain -= r;

            for ( auto it = m_args.begin(); it != m_args.end(); ++it) {
                int len = it->length();
                r = snprintf(&m_buf[pos], remain, "$%d\r\n", len);
                pos += r; remain -= r;
                memcpy(&m_buf[pos], it->data(), len);
                pos += len; remain -= len;
                m_buf[pos] = '\r';
                m_buf[pos + 1] = '\n';
                pos += 2; remain -= 2;
            } 
            m_buf.resize( total_len - remain );  // 去掉多分配的
        }

        int r = m_sh(&m_buf[0], (int)m_buf.size(), m_timeout, e);
        if ( r != (int)m_buf.size() ) return false;

        int  len = 0;   // 收到的长度
        int  pos = 0;   // 每次解析位置
        
        // 接收回复数据并解码到value中
        ValueDecoder decoder(m_rh);
        return decoder.parse(value, m_timeout, e);
    }

    int ValueDecoder::receiveData(int timeout, err::Error * e)
    {
        if ( m_len == m_buf.size() ) m_buf.resize(m_buf.size() * 2);
        int r = m_rh(&m_buf[m_len], (int)m_buf.size() - m_len, timeout, e);
        if ( r > 0 ) m_len += r;
        return r;
    }

    int ValueDecoder::parse(Value * value, int timeout, err::Error * e)
    {
        if ( m_buf.size() < 128) m_buf.resize(128);
        m_pos = 0;
        m_len = 0;

        // 循环收消息，直到收到至少一个字符，或者异常退出
        while ( m_len < 1 ) {
            int n = m_rh(&m_buf[m_len], (int)m_buf.size() - m_len, timeout, e);
            if ( n < 0 ) return -1;   // 异常退出
            m_len += n;
        }
        SYM_TRACE_VA("+++ type: %c", m_buf[0]);
        if ( m_buf[0] == '+' || m_buf[0] == '-')
            return this->parseStatus(value, timeout, e);
        else if ( m_buf[0] == '$') 
            return this->parseBulkStrings(value, timeout, e);
        else if ( m_buf[0] == ':')
            return this->parseInteger(value, timeout, e);
        else if ( m_buf[0] == '*')
            return this->parseArray(value, timeout, e);
        else {
            if ( e ) {
                char err[64];
                snprintf(err, 64, "first char error %c", m_buf[0]);
                *e = err::Error(-1, "first char error");
            }
            return -1;
        }

        return 0;
    }

    int ValueDecoder::parseStatus(Value * value, int timeout, err::Error * e)
    {
        bool status = false;
        if ( m_buf[m_pos] == '+' ) status = true;
        m_pos += 1;

        int end = m_pos;

        while (1) {
            SYM_TRACE_VA("parseStatus while %d", end);
            for(; end < m_len && m_buf[end] != '\n'; ++end) ;
            if ( end < m_len ) break; 

            if ( m_len == m_buf.size() ) m_buf.resize(m_buf.size() * 2);
            int r = m_rh(&m_buf[m_len], (int)m_buf.size() - m_len, timeout, e);
            if ( r < 0 ) return -1;   // 异常退出
            m_len += r;
        }
        
        value->setStatus(status, &m_buf[m_pos], end - m_pos - 1);
        return end + 1;
    }

    int ValueDecoder::parseInteger(Value * value, int timeout, err::Error * e)
    {
        assert(m_buf[m_pos] == ':');
        m_pos += 1;
        
        int end = m_pos;
        while (1) {
            for( ; end < m_len && m_buf[end] != '\n'; ++end) ;
            if ( end < m_len ) break;
            
            int r = this->receiveData(timeout, e);
            if ( r < 0 ) return -1;   // 异常退出
        }
        
        int64_t n = atoll(&m_buf[m_pos]);
        value->setInt(n);
        return end + 1;
    }

    int ValueDecoder::parseBulkStrings(Value * value, int timeout, err::Error * e)
    {
        assert(m_buf[m_pos] == '$');
        m_pos += 1;
        int end = m_pos;

        // 读size部分
        while (1) {
            for( ; end < m_len && m_buf[end] != '\n'; ++end) ;
            if ( end < m_len ) break;
            
            int r = this->receiveData(timeout, e);
            if (r < 0 ) return -1;
        }

        int size = atoi(&m_buf[ m_pos ]);   // 获取到长度。
        if ( size < 0 ) {
            value->setNull();
            return end + 1;
        }

        // 读字符串
        m_pos = end + 1;

        while (1) {
            if ( m_len >= m_pos + size + 2 ) {
                if (m_buf[m_pos + size + 1] == '\n') {
                    break;
                } else {
                    if ( e )  *e  = err::Error(-1, "bad bulk string format");
                    return -1;
                }
            }

            int r = this->receiveData(timeout, e);
            if (r < 0 ) return -1;
        }
        
        value->setString(&m_buf[m_pos], size);
        return m_pos + size + 2;
    }

    int ValueDecoder::parseArray(Value * value, int timeout, err::Error * e)
    {
        assert(m_buf[m_pos] == '*');
        m_pos += 1;
        int end = m_pos;
        value->setType(Value::vtList);

        // array length
        while (1) {
            for( ; end < m_len && m_buf[end] != '\n'; ++end) ;
            if ( end < m_len ) break;
            
            int r = this->receiveData(timeout, e);
            if ( r < 0 ) return -1;   // 异常退出
        }

        int n = atoi( &m_buf[m_pos]);
        if ( n < 0 ) {
            value->setNull();
            return end + 1;
        }
        m_pos = end + 1;

        for ( int i = 0; i < n; ++i ) {
            while ( m_pos >= m_len ) {
                int r = this->receiveData(timeout, e);
                if ( r < 0 ) return -1;
            }

            Value value1;
            if ( m_buf[m_pos] == '+' || m_buf[m_pos] == '-') {
                int r = this->parseStatus(&value1, timeout, e);
                if ( r < 0  )  return -1;
                m_pos = r;
            } 
            else if ( m_buf[m_pos] == '$') {
                int r = this->parseBulkStrings(&value1, timeout, e);
                if ( r < 0 ) return -1;
                m_pos = r;
            }
            else if ( m_buf[m_pos] == ':') {
                int r = this->parseInteger(&value1, timeout, e);
                if ( r < 0 ) return -1;
                m_pos = r;
            }
            else if ( m_buf[m_pos] == '*') {
                int r = this->parseArray(&value1, timeout, e);
                if ( r < 0 ) return -1;
                m_pos = r;
            }
            else {
                if ( e ) {
                    char err[64];
                    snprintf(err, 64, "first char error %c", m_buf[m_pos]);
                    *e = err::Error(-1, "first char error");
                }
                return -1;
            }
            value->list().push_back(value1);
        }
        return m_pos;
    }
} // end namespace redis

END_SYM_NAMESPACE 