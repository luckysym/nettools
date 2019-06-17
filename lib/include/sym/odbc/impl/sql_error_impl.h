#pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{
    struct SQLErrorImpl
    {
        SQLRETURN   m_rcode;
        SQLINTEGER  m_ecode;
        std::string m_state;
        std::string m_text;
        std::string m_func;
        std::string m_hint;

        SQLErrorImpl(const char * func, int rc, int ec, const char * state, const char * text, const char *hint)
            : m_rcode(rc), m_ecode(ec)
            , m_state(state?state:""), m_text(text?text:"")
            , m_func(func?func:""), m_hint(hint?hint:"") 
        {}

    }; // end class SQLErrorIpl

} // end namespace odbc

END_SYM_NAMESPACE