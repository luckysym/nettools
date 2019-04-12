#pragma once 

#include <stdint.h>
#include <sys/time.h>

/// 包含时钟相关操作的名称空间。
namespace chrono
{
    int64_t now();
}

inline
int64_t chrono::now() 
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}