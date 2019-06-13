#pragma once

# include <sym/symdef.h>
# include <sym/utilities.h>
# include <assert.h>
# include <sqltypes.h>
# include <sql.h>
# include <sqlext.h>

# include <sstream>
# include <string>

BEGIN_SYM_NAMESPACE

namespace odbc
{
    class SQLError {
    public:
        static const int ecSuccess       = SQL_SUCCESS;
        static const int ecSuccessInfo   = SQL_SUCCESS_WITH_INFO;
        static const int ecInvalidHandle = SQL_INVALID_HANDLE;
        static const int ecError         = SQL_ERROR;
        
    private:
        SQLRETURN   m_rcode;
        SQLINTEGER  m_ecode;
        std::string m_state;
        std::string m_text;

    public:
        SQLError() : m_rcode(SQL_SUCCESS) {}
        SQLError(int rc) : m_rcode(rc) {} 
        SQLError(int rc, int ec, const char * state, const char * text);

        int rcode() const { return m_rcode; }
        int ecode() const { return m_ecode; }

        std::string str() const; 

    public:
        static SQLError makeError(SQLRETURN r, SQLSMALLINT htype, SQLHANDLE h);
        static const char * rcodeName(SQLRETURN r);
    }; // end class 

    class SQLHandle 
    {
        SYM_NONCOPYABLE(SQLHandle)
    private:
        SQLHANDLE   m_handle;
        SQLSMALLINT m_htype;
    
    protected:
        SQLHandle() : m_handle(SQL_NULL_HANDLE) {}
        SQLHandle(SQLHandle && other);

        bool init(SQLSMALLINT type, SQLHANDLE hIn, SQLSMALLINT hInType, SQLError *e);
        bool initEnv(SQLError *e);

    public:
        virtual ~SQLHandle();
        SQLHANDLE handle() const { return m_handle; }
 
    }; // end class SQLHandle

    class SQLEnvironment : public SQLHandle
    {
    public:
        SQLEnvironment() {}
        SQLEnvironment(SQLError *e) ;
        virtual ~SQLEnvironment() {}

        bool init(SQLError * e);

        bool SetDefaultVersion(SQLError *e);
    }; // end class SQLEnvironment

    class SQLConnection : public SQLHandle
    {
    public:
        SQLConnection() {}
        SQLConnection(SQLEnvironment * env, SQLError * e);
        virtual ~SQLConnection() {}

        bool init(SQLEnvironment * env, SQLError * e);
    }; // end classs SQLConnection

} // end namespace

namespace odbc {

    SQLError::SQLError(int rc, int ec, const char * state, const char * text)
        : m_rcode(rc), m_ecode(ec), m_state(state), m_text(text) 
    {}

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

    std::string SQLError::str() const
    {   
        std::ostringstream os;
        os<<SQLError::rcodeName(m_rcode)<<':'<<m_ecode<<':'<<m_state<<':'<<m_text;
        return os.str();
    }

    SQLError SQLError::makeError(SQLRETURN r, SQLSMALLINT htype, SQLHANDLE h)
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
            return SQLError(r, ec, (const char *)state, (const char *)text.data());
        } else {
            return SQLError(r);
        }
    }

} // end namespace odbc

namespace odbc {
    SQLConnection::SQLConnection(SQLEnvironment * env, SQLError *e)
    {
        this->init(env, e);
    }

    bool SQLConnection::init(SQLEnvironment *env, SQLError * e)
    {
        return this->SQLHandle::init(SQL_HANDLE_DBC, env->handle(), SQL_HANDLE_ENV, e);
    }

} // end namespace odbc


namespace odbc {
    SQLHandle::SQLHandle(SQLHandle && other) 
        : m_handle ( other.m_handle )
    {
        other.m_handle = SQL_NULL_HANDLE;
    }

    SQLHandle::~SQLHandle() {
        if ( m_handle != SQL_NULL_HANDLE ) {
            SQLFreeHandle(m_htype, m_handle);
            m_handle = SQL_NULL_HANDLE;
        }
    }

    bool SQLHandle::init(SQLSMALLINT type, SQLHANDLE hIn, SQLSMALLINT hInType, SQLError *e)
    {
        SQLRETURN r = SQLAllocHandle(type, hIn, &m_handle);
        m_htype = type;

        if ( r == SQL_SUCCESS ) {
            return true;
        } 
        else if ( r == SQL_ERROR || r == SQL_SUCCESS_WITH_INFO || r == SQL_INVALID_HANDLE)
        {
            *e = SQLError::makeError(r, hInType, hIn);
            return false;
        } else {
            assert("SYM::ODBC::SQLHANDLE unknow return value" == nullptr);
        }
    }

    bool SQLHandle::initEnv(SQLError *e)
    {
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_handle);
        
        if ( r == SQL_SUCCESS ) {
            m_htype = SQL_HANDLE_ENV;
            return true;
        } 
        else if ( r == SQL_ERROR || r == SQL_SUCCESS_WITH_INFO || r == SQL_INVALID_HANDLE)
        {
            *e = SQLError::makeError(r, SQL_HANDLE_ENV, SQL_NULL_HANDLE);
            return false;
        } else {
            assert("SYM::ODBC::SQLHANDLE unknow return value" == nullptr);
        }
    }

} // 

namespace odbc {
    
} // end namespace odbc

namespace odbc {
    
    SQLEnvironment::SQLEnvironment(SQLError *e) {
        this->init(e);
    } 

    bool SQLEnvironment::init(SQLError *e) {
        if ( SQLHandle::handle() == SQL_NULL_HANDLE ) {
            return this->SQLHandle::initEnv(e);
        }
        return true;
    }

    bool SQLEnvironment::SetDefaultVersion(SQLError *e)
    {
        SQLRETURN r = SQLSetEnvAttr(
            this->handle(), SQL_ATTR_ODBC_VERSION, 
            (SQLPOINTER)SQL_OV_ODBC3, SQL_NTS);
        if ( r == SQL_SUCCESS ) {
            return true;
        } 
        else if ( r == SQL_ERROR || r == SQL_SUCCESS_WITH_INFO || r == SQL_INVALID_HANDLE)
        {
            *e = SQLError::makeError(r, SQL_HANDLE_ENV, this->handle());
            return false;
        } else {
            assert("SYM::ODBC::SQLHANDLE unknow return value" == nullptr);
        }
    }
} //end namespace odbc


END_SYM_NAMESPACE
