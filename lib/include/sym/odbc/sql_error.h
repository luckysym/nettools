#pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{
    class SQLErrorImpl;

    /// SQL错误类，用于表示ODBC接口执行的错误信息。
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
        operator bool () const;

        int rcode() const;
        int ecode() const;

        std::string str() const; 

    public:
        static SQLError makeError(const char * func, SQLRETURN r, SQLSMALLINT htype, SQLHANDLE h);
        static const char * rcodeName(SQLRETURN r);
    }; // end class SQLError

} // end namespace odbc

END_SYM_NAMESPACE

# include <sym/odbc/impl/sql_error_impl.h>

