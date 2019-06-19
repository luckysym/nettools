#pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{
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

END_SYM_NAMESPACE
