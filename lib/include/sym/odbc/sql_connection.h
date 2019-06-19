#pragma once

# include <sym/symdef.h>


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
        bool setAutoCommit(bool autoCommit, SQLError * e);
        bool commit(SQLError * e);
        bool rollback(SQLError * e);
        
    }; // end classs SQLConnection
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

    SYM_INLINE 
    bool SQLConnection::setAutoCommit(bool autoCommit, SQLError * e)
    {
        SQLUINTEGER value = autoCommit?SQL_AUTOCOMMIT_ON :SQL_AUTOCOMMIT_OFF;

        SQLRETURN r =  SQLSetConnectAttr(
            this->handle(),
            SQL_ATTR_AUTOCOMMIT,
            (SQLPOINTER)(int64_t)value,
            SQL_IS_UINTEGER);
        return SYM_ODBC_MAKE_RETURN(
            "SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT)", 
            r, e, SQL_HANDLE_DBC, this->handle());
    }

    SYM_INLINE
    bool SQLConnection::commit(SQLError * e)
    {   
        SQLRETURN r = SQLEndTran(SQL_HANDLE_DBC, this->handle(), SQL_COMMIT );
        return SYM_ODBC_MAKE_RETURN("SQLEndTran(SQL_COMMIT)", r, e, SQL_HANDLE_DBC, this->handle());
    }

    SYM_INLINE
    bool SQLConnection::rollback(SQLError * e)
    {
        SQLRETURN r = SQLEndTran(SQL_HANDLE_DBC, this->handle(), SQL_ROLLBACK );
        return SYM_ODBC_MAKE_RETURN("SQLEndTran(SQL_ROLLBACK)", r, e, SQL_HANDLE_DBC, this->handle());
    }

} // end namespace odbc

END_SYM_NAMESPACE