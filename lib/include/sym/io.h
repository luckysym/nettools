/**
 * 本文件声明了基本IO操作所需的数据结构和函数。
 */
#pragma once 

#include <stdlib.h>
#include <assert.h>
#include <memory.h>

namespace io {
    /// IO缓存
    typedef struct buffer {
        char *    data;
        size_t    size;
        size_t    begin;
        size_t    end;
    } buffer_t;

    /// extend the buffer capacity to the new size
    buffer_t * buffer_alloc(buffer_t *buf, size_t size);

    /// free the buffer members, release its memory
    void buffer_free(buffer_t *buf);

    /// realloc the buffer with the new capacity size.
    buffer_t * buffer_realloc(buffer_t *buf, size_t size);

    /// move the data in the buffer to the front
    buffer_t * buffer_pullup(buffer_t *buf);

} // end namespace io

inline 
void io::buffer_free(io::buffer_t *buf) 
{
    if ( buf->data ) free(buf->data);
    buf->data = nullptr;
    buf->size = 0;
    buf->begin = 0;
    buf->end = 0;
}

inline 
io::buffer_t * io::buffer_alloc(io::buffer_t *buf, size_t size)
{
    if ( size > 0 ) {
        buf->data = (char *)malloc(size);
        assert(buf->data);
    } else {
        buf->data = nullptr;
    }
    buf->size  = size;
    buf->begin = 0;
    buf->end   = 0;
    return buf;
}

inline 
io::buffer_t * io::buffer_realloc(io::buffer_t *buf, size_t size)
{
    if ( size == 0 ) {
        io::buffer_free(buf);
        return buf;
    }
    void * p = realloc(buf->data, size);
    buf->data = (char*)p;
    buf->size = size;
    if ( buf->begin > buf->size ) buf->begin = buf->size;
    if ( buf->end > buf->size ) buf->end = buf->size;
    return buf;
}

inline 
io::buffer_t * io::buffer_pullup(io::buffer_t *buf) 
{
    if ( buf->begin == 0 ) return buf;

    size_t len = buf->end - buf->begin;
    void * p = memmove(buf->data, buf->data + buf->begin, len);
    assert(p == buf->data);
    buf->begin = 0;
    buf->end = len;
    return buf;
}