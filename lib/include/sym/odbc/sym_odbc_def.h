# pragma once

# include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

namespace odbc
{

} // end namespace odbc

END_SYM_NAMESPACE

/// 根据ODBC API函数的结果生成返回值, 并且在有错误信息的时候设置SQLError。
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


