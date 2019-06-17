# pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{
    class SQLEnvironmentImpl;
    class SQLEnvironment : public SQLHandle
    {
    private:
        SQLEnvironmentImpl * m_impl;
    public:
        SQLEnvironment();
        SQLEnvironment(SQLError *e) ;
        virtual ~SQLEnvironment();

        bool init(SQLError * e);

    }; // end class SQLEnvironment
} // end namespace odbc

END_SYM_NAMESPACE

# include <sym/odbc/impl/sql_environment_impl.h>
