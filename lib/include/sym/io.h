#pragma once 

#include <stdlib.h>
#include <assert.h>
#include <memory.h>

/// 包含基本IO操作和数据结构的名称空间。
namespace io {
    /// IO缓存
    typedef struct buffer {
        char *    data;     ///< 内存指针。
        size_t    size;     ///< 缓存总大小。
        size_t    begin;    ///< 缓存有效数据开始位置（含）。
        size_t    end;      ///< 缓存有效数据结束位置（不含）。
        size_t    limit;    ///< 缓存写入或读取位置限制位置（不含）。
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

    /// big endian to host endian
    int32_t btoh(int32_t n);
    int64_t btoh(int64_t n);

    class ConstBuffer {};
    class MutableBuffer {
    public:
        char * data();
        size_t capacity() const;
        size_t size() const;
        size_t limit() const;

        void resize(size_t n);
        void limit(size_t n);

        void attach(char * p, size_t cap);
        char * detach();

        void rewind();
    };

} // end namespace io

inline 
int32_t io::btoh(int32_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int32_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    p1[0] = p0[3];
    p1[1] = p0[2];
    p1[2] = p0[1];
    p1[3] = p0[0];
    return r;
# endif
}

inline 
int64_t io::btoh(int64_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int64_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    p1[0] = p0[7];
    p1[1] = p0[6];
    p1[2] = p0[5];
    p1[3] = p0[4];
    p1[4] = p0[3];
    p1[5] = p0[2];
    p1[6] = p0[1];
    p1[7] = p0[0];
    return r;
# endif
}

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