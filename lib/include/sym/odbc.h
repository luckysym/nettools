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

/// 根据ODBC API函数的结果生成返回值。
# define SYM_ODBC_MAKE_RETURN(func, r, e, htype, handle) \
    ({    \
        bool isok;  \
        if ( r == SQL_ERROR || r == SQL_SUCCESS_WITH_INFO) { \
            *e = SQLError::makeError(func, r, htype, handle); \
        } \
        if ( r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO ) isok = true; \
        else isok = false;  \
        isok;   \
    })

/// odbc名称空间，包含对ODBC操作的封装。
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
        std::string m_func;

    public:
        SQLError() : m_rcode(SQL_SUCCESS) {}
        SQLError(int rc) : m_rcode(rc) {} 
        SQLError(const char * func, int rc, int ec, const char * state, const char * text);

        int rcode() const { return m_rcode; }
        int ecode() const { return m_ecode; }

        std::string str() const; 

    public:
        static SQLError makeError(const char * func, SQLRETURN r, SQLSMALLINT htype, SQLHANDLE h);
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

    }; // end class SQLEnvironment

    class SQLConnection : public SQLHandle
    {
    public:
        SQLConnection() {}
        SQLConnection(SQLEnvironment * env, SQLError * e);
        virtual ~SQLConnection() {}

        bool init(SQLEnvironment * env, SQLError * e);
        bool open(const char * connstr, const char *user, const char * pwd, SQLError *e);
        bool close(SQLError *e);
    }; // end classs SQLConnection

    /// SQL Statement封装类
    class SQLStatement : public SQLHandle 
    {
    public:
        SQLStatement() {}
        SQLStatement(SQLConnection * conn, SQLError * e);
        virtual ~SQLStatement() {}

        bool init(SQLConnection *conn, SQLError *e);
        bool exec(const char * sql, SQLError *e);
        bool prepare(const char * sql, SQLError * e);
        bool execute(SQLError * e);
    }; // end class SQLStatement

} // end namespace

namespace odbc {

    SYM_INLINE
    SQLStatement::SQLStatement(SQLConnection * conn, SQLError * e)
    {
        this->init(conn, e);
    }

    SYM_INLINE
    bool SQLStatement::init(SQLConnection *conn, SQLError *e)
    {
        return this->SQLHandle::init(SQL_HANDLE_STMT, conn->handle(), SQL_HANDLE_DBC, e);
    }

    SYM_INLINE
    bool SQLStatement::exec(const char * sql, SQLError * e) 
    {
        SQLRETURN r = SQLExecDirect(this->handle(), (SQLCHAR*)sql, SQL_NTS);
        return SYM_ODBC_MAKE_RETURN("SQLExecDirect", r, e, SQL_HANDLE_STMT, this->handle());
    }

    SYM_INLINE
    bool SQLStatement::prepare(const char * sql, SQLError * e)
    {
        SQLRETURN r = SQLPrepare(this->handle(), (SQLCHAR*)sql, SQL_NTS);
        return SYM_ODBC_MAKE_RETURN("SQLPrepare", r, e, SQL_HANDLE_STMT, this->handle());
    }

    SYM_INLINE
    bool SQLStatement::execute(SQLError * e)
    {
        SQLRETURN r = SQLExecute(this->handle() );
        return SYM_ODBC_MAKE_RETURN("SQLExecute", r, e, SQL_HANDLE_STMT, this->handle());
    }
} // end namespace odbc

namespace odbc {

    SYM_INLINE
    SQLError::SQLError(const char * func, int rc, int ec, const char * state, const char * text)
        : m_rcode(rc), m_ecode(ec), m_state(state), m_text(text), m_func(func)
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

namespace odbc {
    SYM_INLINE
    SQLConnection::SQLConnection(SQLEnvironment * env, SQLError *e)
    {
        this->init(env, e);
    }

    SYM_INLINE
    bool SQLConnection::init(SQLEnvironment *env, SQLError * e)
    {
        return this->SQLHandle::init(SQL_HANDLE_DBC, env->handle(), SQL_HANDLE_ENV, e);
    }

    SYM_INLINE
    bool SQLConnection::open(const char * dsn, const char *user, const char * pwd, SQLError *e)
    {
        SQLRETURN r = SQLConnect(
            this->handle(), (SQLCHAR*)dsn, SQL_NTS, (SQLCHAR*)user, SQL_NTS, (SQLCHAR*)pwd, SQL_NTS);

        return SYM_ODBC_MAKE_RETURN("SQLConnect", r, e, SQL_HANDLE_DBC, this->handle());
    }

    SYM_INLINE
    bool SQLConnection::close(SQLError * e) 
    {
        SQLRETURN r = SQLDisconnect(this->handle());
        return SYM_ODBC_MAKE_RETURN("SQLDisconnect", r, e, SQL_HANDLE_DBC, this->handle());
    }

} // end namespace odbc


namespace odbc {

    SYM_INLINE
    SQLHandle::SQLHandle(SQLHandle && other) 
        : m_handle ( other.m_handle )
    {
        other.m_handle = SQL_NULL_HANDLE;
    }

    SYM_INLINE
    SQLHandle::~SQLHandle() {
        if ( m_handle != SQL_NULL_HANDLE ) {
            SQLFreeHandle(m_htype, m_handle);
            m_handle = SQL_NULL_HANDLE;
        }
    }

    SYM_INLINE
    bool SQLHandle::init(SQLSMALLINT type, SQLHANDLE hIn, SQLSMALLINT hInType, SQLError *e)
    {
        SQLRETURN r = SQLAllocHandle(type, hIn, &m_handle);
        m_htype = type;

        return SYM_ODBC_MAKE_RETURN("SQLAllocHandle", r, e, hInType, hIn);
    }

    SYM_INLINE
    bool SQLHandle::initEnv(SQLError *e)
    {
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_handle);
        
        if ( r == SQL_SUCCESS ) m_htype = SQL_HANDLE_ENV;
        
        return SYM_ODBC_MAKE_RETURN("SQLAllocHandle", r, e, SQL_HANDLE_ENV, SQL_NULL_HANDLE);
    }

} // 

namespace odbc {
    
} // end namespace odbc

namespace odbc {
    
    SYM_INLINE
    SQLEnvironment::SQLEnvironment(SQLError *e) {
        this->init(e);
    } 

    SYM_INLINE
    bool SQLEnvironment::init(SQLError *e) {
        if ( SQLHandle::handle() == SQL_NULL_HANDLE ) {
            SQLRETURN r = this->SQLHandle::initEnv(e);
            if ( r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO ) return false;
        
            r = SQLSetEnvAttr(this->handle(), SQL_ATTR_ODBC_VERSION, 
                (SQLPOINTER)SQL_OV_ODBC3, SQL_NTS);

            return SYM_ODBC_MAKE_RETURN("SQLSetEnvAttr", r, e, SQL_HANDLE_ENV, this->handle());
        }
        return true;
    }

} //end namespace odbc


END_SYM_NAMESPACE
