#pragma once 

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// 包含异常错误以及过程跟踪相关数据结构和操作的名称空间。
namespace err {
    typedef struct error_info {
        char * str;
        struct error_info * next;
    } error_t;

    /// init the error info
    void init_error_info(error_t * err);

    /// free error info members
    void free_error_info(error_t * err);

    /// push error info to the error object
    void push_error_info(error_t * err, int size, const char * format, ...);

    // move src error to dest
    void move_error_info(error_t * dest, error_t *src);

    void trace_stderr(const char * file, int line, const char * format, ...);

    class Error { };

} // end namespace sl

/// \def SYM_TRACE(msg)
/// 用于输出过程跟踪信息。
#define SYM_TRACE(msg) err::trace_stderr(__FILE__, __LINE__, msg) 
#define SYM_TRACE_VA(fmt, ...) err::trace_stderr(__FILE__, __LINE__, fmt, __VA_ARGS__) 

#define SYM_NOTIMPL(name) assert( name" not impl" == nullptr)

inline
void err::trace_stderr(const char * file, int line, const char * format, ...)
{
    char * file1 = strrchr((char *)file , '/');
    if ( file1 == nullptr ) file1 = (char*)file;
    else file1 += 1;
    printf("%s:%d ", file1, line);

    va_list ap;
    va_start(ap, format);
    int n = vprintf(format, ap);
    va_end(ap);

    printf("\n");
}

inline 
void err::init_error_info(err::error_info * err)
{
    err->str = nullptr;
    err->next = nullptr;
}

inline 
void err::free_error_info(err::error_info * err)
{
    error_t * p = err->next;
    while ( p ) {
        error_t * p1 = p->next;
        if ( p->str ) {
            free(p->str);
            p->str = nullptr;
        }
        p->next = nullptr;
        free(p);
        p = p1;
    }

    if ( err->str ) {
        free((void *)err->str);
        err->str = nullptr;
    }
    err->next = nullptr;
}

inline 
void err::push_error_info(err::error_info * err, int size, const char *format, ...) {
    if( err->str ) {
        err->next = (error_t*)malloc(sizeof(error_t));
        err->next->str = err->str;
    }

    err->str = (char*)malloc(size);
    va_list ap;
    va_start(ap, format);
    vsnprintf(err->str, size, format, ap);
    va_end(ap);
}

inline 
void err::move_error_info(error_t * dest, error_t *src) {
    free_error_info(dest);
    dest->str = src->str;
    dest->next = src->next;
    src->str = nullptr;
    src->next = nullptr;
}