#pragma once 

#include <stdlib.h>
#include <assert.h>
#include <memory.h>

/// 包含基本IO操作和数据结构的名称空间。
namespace io {
    /// IO缓存
    typedef struct buffer {
        char *    data;
        size_t    size;
        size_t    limit;
        size_t    begin;
        size_t    end;
    } buffer_t;

    /// init the buffer struct
    buffer_t * buffer_init(buffer_t *buf);

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
io::buffer_t * io::buffer_init(io::buffer_t *buf)
{
    buf->data = nullptr;
    buf->size = buf->begin = buf->end = buf->limit = 0;
    return buf;
}

inline 
void io::buffer_free(io::buffer_t *buf) 
{
    if ( buf->data ) free(buf->data);
    buf->data  = nullptr;
    buf->size  = 0;
    buf->limit = 0;
    buf->begin = 0;
    buf->end   = 0;
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
    buf->size  = buf->limit = size;
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
    if ( buf->limit > buf->end ) buf->limit = buf->end;

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