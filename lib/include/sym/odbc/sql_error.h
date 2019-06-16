#pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{

    class SQLError {
    public:
        static const int rcSuccess       = SQL_SUCCESS;
        static const int rcSuccessInfo   = SQL_SUCCESS_WITH_INFO;
        static const int rcInvalidHandle = SQL_INVALID_HANDLE;
        static const int rcError         = SQL_ERROR;
        
    private:
        SQLRETURN   m_rcode;
        SQLINTEGER  m_ecode;
        std::string m_state;
        std::string m_text;
        std::string m_func;
        std::string m_hint;

    public:
        SQLError() : m_rcode(SQL_SUCCESS) {}
        SQLError(int rc) : m_rcode(rc) {} 
        SQLError(const char * func, int rc, int ec, const char * state, const char * text);
        SQLError(const char * func, int rc, int ec, const char * state, const char * text, const char *hint);

        int rcode() const { return m_rcode; }
        int ecode() const { return m_ecode; }

        std::string str() const; 

    public:
        static SQLError makeError(const char * func, SQLRETURN r, SQLSMALLINT htype, SQLHANDLE h);
        static const char * rcodeName(SQLRETURN r);
    }; // end class SQLError

} // end namespace odbc


namespace odbc
{

    SYM_INLINE
    SQLError::SQLError(const char * func, int rc, int ec, const char * state, const char * text)
        : m_rcode(rc), m_ecode(ec), m_state(state), m_text(text), m_func(func)
    {}

    SYM_INLINE
    SQLError::SQLError(const char * func, int rc, int ec, const char * state, const char * text, const char *hint)
        : m_rcode(rc), m_ecode(ec), m_state(state), m_text(text), m_func(func), m_hint(hint)
    {}

    SYM_INLINE
    const char * SQLError::rcodeName(SQLRETURN r)
    {
        switch (r) {
            case SQL_SUCCESS: return "SQL_SUCCESS";
            case SQL_ERROR: return "SQL_ERROR";
            case SQL_SUCCESS_WITH_INFO: return "SQL_SUCCESS_WITH_INFO";
            case SQL_INVALID_HANDLE: return "SQL_INVALID_HANDLE";
            default:
                assert(false);
        }
    }

    SYM_INLINE
    std::string SQLError::str() const
    {   
        std::ostringstream os;
        os<<m_func<<':'<<SQLError::rcodeName(m_rcode)<<':'<<m_ecode<<':'<<m_state<<':'<<m_text;
        if ( !m_hint.empty() ) os<<m_hint;
        return os.str();
    }

    SYM_INLINE
    SQLError SQLError::makeError(const char * func, SQLRETURN r, SQLSMALLINT htype, SQLHANDLE h)
    {
        SQLCHAR state[8];
        SQLINTEGER ec;

        util::Array<SQLCHAR> text(256);
        state[5] = '\0';
        SQLRETURN r1;
        while (1) {
            SQLSMALLINT len;
            r1 = SQLGetDiagRec(
                htype, h, 1, (SQLCHAR*)state, &ec, (SQLCHAR*)text.data(), (SQLSMALLINT)text.size(), &len);
            if ( r1 == SQL_SUCCESS_WITH_INFO) {
                size_t tlen = text.size();
                text = util::Array<SQLCHAR>(tlen * 2);
            } else {    
                break;
            }
        }

        if ( r1 == SQL_SUCCESS ) {
            return SQLError(func, r, ec, (const char *)state, (const char *)text.data());
        } else {
            return SQLError(r);
        }
    }

} // end namespace odbc


END_SYM_NAMESPACE

