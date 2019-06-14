#pragma once

# include <sym/symdef.h>
# include <sym/utilities.h>
# include <assert.h>
# include <string.h>

# include <sqltypes.h>
# include <sql.h>
# include <sqlext.h>

# include <sstream>
# include <string>

# include <sym/odbc/sym_odbc_def.h>
# include <sym/odbc/sql_error.h>
# include <sym/odbc/sql_handle.h>
# include <sym/odbc/sql_environment.h>

BEGIN_SYM_NAMESPACE

/// odbc名称空间，包含对ODBC操作的封装。
namespace odbc
{
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

    class SQLParameter
    {
    private:
        util::Array<char>   m_buffer;
        size_t              m_bufsize;
        SQLLEN              m_lenOrInd;
        SQLSMALLINT         m_inout;
        SQLSMALLINT         m_ctype;
        SQLSMALLINT         m_dbtype;
        SQLULEN             m_colsize;
        SQLSMALLINT         m_decdigits;

    public:
        SQLParameter(SQLSMALLINT inOrOut, SQLSMALLINT ctype, SQLSMALLINT dbtype, SQLULEN colsize, SQLSMALLINT decdigits);
        virtual ~SQLParameter() {}

        bool bind(SQLStatement * stmt, int paramNum, SQLError * e);

    protected:
        void setValue(const void * ptr, size_t len);
    }; // end class SQLParameter

    class SQLStringParameter : public SQLParameter
    {
    public:
        SQLStringParameter(const char * value);
        virtual ~SQLStringParameter() {}
    }; // end class SQLStringParameter

} // end namespace odbc

namespace odbc {

    SYM_INLINE
    SQLParameter::SQLParameter(SQLSMALLINT inOrOut, SQLSMALLINT ctype, SQLSMALLINT dbtype, SQLULEN colsize, SQLSMALLINT decdigits)
        : m_inout(inOrOut), m_ctype(ctype), m_dbtype(dbtype), m_colsize(colsize), m_decdigits(decdigits)
    { }

    SYM_INLINE
    void SQLParameter::setValue(const void * ptr, size_t len)
    {
        util::Array<char> newArray(len);
        m_buffer = std::move(newArray);
        if ( len <= 16 ) for(int i = 0; i < len; ++i) m_buffer[i] = reinterpret_cast<const char*>(ptr)[i];
        else memcpy(m_buffer.data(), ptr, len);
        m_lenOrInd = len;
    }

    SYM_INLINE
    bool SQLParameter::bind(SQLStatement * stmt, int paramNum, SQLError * e)
    {
        SQLRETURN r = SQLBindParameter(
            stmt->handle(),
            paramNum, 
            m_inout,
            m_ctype,
            m_dbtype,
            m_colsize,
            m_decdigits,
            (SQLPOINTER)m_buffer.data(),
            m_bufsize,
            &m_lenOrInd
        );
        return SYM_ODBC_MAKE_RETURN("SQLBindParameter", r, e, SQL_HANDLE_STMT, stmt->handle());
    }

} // end namespace odbc

namespace odbc 
{
    SYM_INLINE
    SQLStringParameter::SQLStringParameter(const char * value) 
        : SQLParameter(SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, SQL_NTS, 0)
    {
        SQLParameter::setValue(value, strlen(value));
    }

} // end namespace odbc

namespace odbc 
{

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

END_SYM_NAMESPACE
