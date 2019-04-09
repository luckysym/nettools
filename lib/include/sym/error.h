#pragma once 

#include <stdarg.h>
#include <stdio.h>

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

    void trace_stderr(const char * file, int line, const char * format, ...);

} // end namespace sl

#define SYM_TRACE(msg) err::trace_stderr(__FILE__, __LINE__, msg) 
#define SYM_TRACE_VA(fmt, ...) err::trace_stderr(__FILE__, __LINE__, fmt, __VA_ARGS__) 

inline
void err::trace_stderr(const char * file, int line, const char * format, ...)
{
    printf("%s:%d ", file, line);

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
