# pragma once 

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE 

namespace odbc
{
    SYM_INLINE
    SQLEnvironment::SQLEnvironment() : m_impl(nullptr) {}

    SYM_INLINE 
    SQLEnvironment::~SQLEnvironment() {}

    SYM_INLINE
    SQLEnvironment::SQLEnvironment(SQLError *e) : m_impl(nullptr)  {
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
} // end namespace odbc

END_SYM_NAMESPACE