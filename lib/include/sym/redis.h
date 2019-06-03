#pragma once 

#include <memory.h>
#include <vector>
#include <functional>
#include <sym/error.h>


namespace redis
{
    class Value 
    {
    private:
        bool        m_status;
        std::string m_str;
    public:
        bool status() const { return m_status; }
        void status(bool status) { m_status = status; }

        void setString(const char * data, int len) {
            m_str.assign(data, len);
        }

        std::string getString() const { return m_str; }
    }; // end class Value

    class ValueDecoder
    {
    private:
        static const int stateInit   = 0;
        static const int stateFinish = 1;
        static const int stateStatus = 2;  // 解析完状态（+, -）
    private:
        int m_state;
        Value * m_val;
    public:
        int  parse(const char * buf, int len, err::Error *e = nullptr);

        bool finished() const { return m_state == stateFinish; }
        void reset(Value * value) {
            m_state = stateInit;
            m_val = value;
        }
    }; // end class ValueDecoder

    class Command 
    {
    public:
        using OnSend = std::function<int (const char * cmd, int len, int timeout, err::Error * e)>;
        using OnRecv = std::function<int (char * buf, int len, int timeout, err::Error *e)>;

    private:
        std::vector<char> m_buf;
        OnSend            m_sh;
        OnRecv            m_rh;
        
    public:
        Command(OnSend sh, OnRecv rh ) : m_sh(sh), m_rh(rh) {}

        bool execute(Value * value, int timeout, err::Error *e = nullptr);
        void setText(const char * text);
        
    }; // end class Command

    void Command::setText(const char * text) 
    {
        if ( text ) {
            int len = strlen(text);
            m_buf.resize( len );
            memcpy(&m_buf[0], text, len);
        } else {
            m_buf.resize(0);
        }
    }

    bool Command::execute(Value * value, int timeout, err::Error * e ) 
    {
        int r = m_sh(&m_buf[0], (int)m_buf.size(), timeout, e);
        if ( r != (int)m_buf.size() ) return false;

# define  RDS_BUF_SIZE 128
        char buf[RDS_BUF_SIZE];
        int  len = 0;
        
        // 接收回复数据并解码到value中
        ValueDecoder decoder;
        decoder.reset(value);
        while (1) {
            int r = m_rh(buf + len, RDS_BUF_SIZE - len, timeout, e);
            if ( r < 0 ) return false; 

            len += r;
            int c = decoder.parse(buf, len, e);
            if ( c < 0 ) return false;

            if ( decoder.finished() ) return true;
            SYM_TRACE_VA("--- %d  %d", r, c);
            
            if ( c > 0 ) {
                len -= c;
                memmove(buf, buf + c, len);
            }
            
        }
    }

    int ValueDecoder::parse(const char * buf, int len, err::Error *e)
    {
        int pos = 0;
        while ( pos < len && m_state != stateFinish)
        {
            SYM_TRACE_VA("pos = %d. state = %d", pos, m_state);
            if ( m_state == stateInit) {
                if ( buf[0] == '+' ) m_val->status(true);
                else if ( buf[0] == '-') m_val->status(false);
                else {
                    SYM_TRACE_VA("*** ERROR status sign: %c", buf[0]);
                    if ( e ) *e = err::Error(-1, "bad status sign");
                    return -1;
                }
                m_state = stateStatus;
            }
            else if ( m_state == stateStatus ) {
                SYM_TRACE_VA("---  %d", pos);
                int i = pos;
                for ( ; i < len && buf[i] != '\n'; ++i )  ;
                if ( buf[i] == '\n') {
                    m_val->setString(buf + pos, i - pos - 1); // 去掉\r
                    pos = i;
                    m_state = stateFinish;
                    SYM_TRACE_VA("state finished = %d", pos);
                } else {
                    SYM_TRACE_VA("return pos1 = %d", pos);
                    return pos;  // 返回已经解析到的位置
                }
            }
            ++pos;
        } // end while
        return pos;
    }
} // end namespace redis