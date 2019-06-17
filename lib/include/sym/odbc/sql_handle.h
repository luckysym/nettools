# pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{
    /// ODBC句柄对象基类，其子类包括SQLEnvironment，SQLConnection，SQLStatement等。
    class SQLHandle 
    {
        SYM_NONCOPYABLE(SQLHandle)
    private:
        SQLHANDLE   m_handle;
        SQLSMALLINT m_htype;
    
    protected:
        SQLHandle() : m_handle(SQL_NULL_HANDLE) {}
        SQLHandle(SQLHandle && other) : m_handle(other.m_handle), m_htype(other.m_htype) 
        {
            other.m_handle = SQL_NULL_HANDLE;
        }

        bool init(SQLSMALLINT type, SQLHANDLE hIn, SQLSMALLINT hInType, SQLError *e);
        bool initEnv(SQLError *e);

    public:
        virtual ~SQLHandle();
        SQLHANDLE   handle() const { return m_handle; }
        SQLSMALLINT handleType() const { return m_htype;  }
 
    }; // end class SQLHandle
} // end namespace odbc

END_SYM_NAMESPACE

# include <sym/odbc/impl/sql_handle_impl.h>