#pragma once

# include <sym/symdef.h>

# include <sym/odbc/impl/sql_error_impl.h>

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
        SQLErrorImpl * m_impl;

    public:
        SQLError();
        SQLError(int rc);
        SQLError(const char * func, int rc, int ec, const char * state, const char * text);
        SQLError(const char * func, int rc, int ec, const char * state, const char * text, const char *hint);
        SQLError(const SQLError &other);
        SQLError(SQLError && other);
        ~SQLError();

        SQLError & operator=(const SQLError &other);
        SQLError & operator=(SQLError && other);

        int rcode() const;
        int ecode() const;

        std::string str() const; 

    public:
        static SQLError makeError(const char * func, SQLRETURN r, SQLSMALLINT htype, SQLHANDLE h);
        static const char * rcodeName(SQLRETURN r);
    }; // end class SQLError

} // end namespace odbc


namespace odbc
{
    SYM_INLINE
    SQLError::SQLError() : m_impl(nullptr) {}

    SYM_INLINE 
    SQLError::SQLError(int rc) : m_impl(new SQLErrorImpl("", rc, 0, "", "", ""))
    {}

    SYM_INLINE
    SQLError::SQLError(const char * func, int rc, int ec, const char * state, const char * text)
        : m_impl(new SQLErrorImpl(func, rc, ec, state, text, ""))
    {}
    
    SYM_INLINE
    SQLError::SQLError(const char * func, int rc, int ec, const char * state, const char * text, const char *hint)
        : m_impl(new SQLErrorImpl(func, rc, ec, state, text, hint))
    {}

    SYM_INLINE
    SQLError::SQLError(const SQLError & other) : m_impl(nullptr)
    {
        if ( other.m_impl ) m_impl = new SQLErrorImpl(*other.m_impl);
    }

    SYM_INLINE
    SQLError::SQLError(SQLError && other) : m_impl(other.m_impl) 
    {
        other.m_impl = nullptr;
    }

    SYM_INLINE 
    SQLError::~SQLError() {
        if ( m_impl ) delete m_impl;
        m_impl = nullptr;
    }

    SYM_INLINE 
    SQLError & SQLError::operator=(const SQLError & other)
    {
        if  ( this != &other) {
            if ( m_impl == nullptr ) m_impl = new SQLErrorImpl(*other.m_impl);
            else *m_impl = *other.m_impl;
        }
        return *this;
    } 

    SYM_INLINE 
    SQLError & SQLError::operator=(SQLError && other) 
    {
        SYM_MOVE_OPERATOR_IMPL();
        return *this;
    }

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
    int SQLError::rcode() const { 
        if ( m_impl ) return m_impl->m_rcode; 
        else return SQL_SUCCESS;
    }
    
    SYM_INLINE
    int SQLError::ecode() const { 
        if ( m_impl ) return m_impl->m_ecode; 
        else return 0;
    }

    SYM_INLINE
    std::string SQLError::str() const
    {   
        if ( m_impl ) {
            std::ostringstream os;
            os<<m_impl->m_func<<':'<<SQLError::rcodeName(m_impl->m_rcode)<<':'
              <<m_impl->m_ecode<<':'<<m_impl->m_state<<':'<<m_impl->m_text;
            if ( !m_impl->m_hint.empty() ) os<<m_impl->m_hint;
            return os.str();
        } else {
            return std::string();
        }
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

