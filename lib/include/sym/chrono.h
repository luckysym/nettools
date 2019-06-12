#pragma once 

#include <stdint.h>
#include <sys/time.h>

#include <sym/symdef.h>

BEGIN_SYM_NAMESPACE

/// 包含时钟相关操作的名称空间。
namespace chrono
{
    ///< 获取并返回当前微秒级时间戳。
    int64_t now();   
}

inline
int64_t chrono::now() 
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

END_SYM_NAMESPACE
