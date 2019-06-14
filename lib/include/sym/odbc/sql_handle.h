# pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{
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
} // end namespace odbc

namespace odbc
{
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
} // end namespace odbc

END_SYM_NAMESPACE