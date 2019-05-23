#pragma once 

#include <stdlib.h>
#include <assert.h>
#include <memory.h>

/// 包含基本IO操作和数据结构的名称空间。
namespace io {

    /// big endian to host endian
    int16_t btoh(int16_t n);
    int32_t btoh(int32_t n);
    int64_t btoh(int64_t n);

    int16_t htob(int16_t n);
    int32_t htob(int32_t n);
    int64_t htob(int64_t n);

    class BufferBase {
    private:
        size_t   m_capacity;
        size_t   m_size;
        size_t   m_position;
        size_t   m_limit;

    public:
        BufferBase() : m_capacity(0), m_size(0), m_position(0), m_limit(0) {}
        BufferBase(size_t size, size_t cap) 
            : m_capacity(cap), m_size(size), m_position(0), m_limit(cap) {}
        
        size_t capacity() const { return m_capacity; }
        size_t size() const     { return m_size;     }
        size_t position() const { return m_position; }
        size_t limit() const    { return m_limit;    }

        void resize(size_t n)   { assert(n <= m_capacity); m_size = n; }
        void position(size_t n) { assert(n <= m_size && n <= m_limit); m_position = n; }
        void limit(size_t n)    { assert(n <= m_capacity); m_limit = n; m_position = m_position > m_limit?m_limit:m_position; }
    
    protected:
        void init() {
            m_capacity = m_size = m_position = m_limit = 0;
        }
    }; // end class BufferBase 

    /// \brief 只读缓存对象，通常用于IO发送数据。
    class ConstBuffer : public BufferBase {
    private:
        const char * m_data { nullptr };

    public:
        ConstBuffer() {}
        ConstBuffer(const char * data, size_t size, size_t cap) 
            : BufferBase(size, cap), m_data(data) 
        {
            BufferBase::limit(size);
        }

        const char * data() const { return m_data; }
        
        void attach(const char *data, size_t size, size_t cap) 
        {
            *(BufferBase*)this = BufferBase(size, cap);
            m_data = data;
        }

        const char * detach() 
        {
            const char * p = m_data;
            m_data = nullptr;
            BufferBase::init();
            return p;
        }

        void rewind() { BufferBase::position(0); }
        
    }; // end class ConstBuffer

    /// \brief 可写入的缓存对象，通常用于接受IO数据。
    class MutableBuffer : public BufferBase {
    private:
        char * m_data { nullptr };
    public:
        MutableBuffer() {}
        MutableBuffer(char * data, size_t size, size_t cap) 
            : BufferBase(size, cap), m_data(data) 
        {}

        char * data() { return m_data; }

        void attach(char *data, size_t size, size_t cap) 
        {
            *(BufferBase*)this = BufferBase(size, cap);
            m_data = data;
        }

        char * detach() 
        { 
            char * p = m_data;
            BufferBase::init();
            m_data = nullptr;
            return p;
        }

        void reset()  { BufferBase::position(0), BufferBase::limit(BufferBase::capacity()); }
    }; // end MutableBuffer

} // end namespace io


#ifndef SYM_BYTEORDER_CONVERT_16
#define SYM_BYTEORDER_CONVERT_16(s, t) \
do {    \
    t[0] = s[1]; \
    t[1] = s[0];   \
} while (0)
#endif // SYM_BYTEORDER_CONVERT_16

#ifndef SYM_BYTEORDER_CONVERT_32
#define SYM_BYTEORDER_CONVERT_32(s, t) \
do {    \
    t[0] = s[3]; \
    t[1] = s[2]; \
    t[2] = s[1]; \
    t[3] = s[0];    \
} while (0) 
#endif // SYM_BYTEORDER_CONVERT_32

#ifndef SYM_BYTEORDER_CONVERT_64
#define SYM_BYTEORDER_CONVERT_64(s, t) \
do { \
    t[0] = s[7];    \
    t[1] = s[6];    \
    t[2] = s[5];    \
    t[3] = s[4];    \
    t[4] = s[3];    \
    t[5] = s[2];    \
    t[6] = s[1];    \
    t[7] = s[0];    \
} while (0)
#endif // SYM_BYTEORDER_CONVERT_64


inline 
int64_t io::btoh(int64_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int64_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    SYM_BYTEORDER_CONVERT_64(p0, p1);
    return r;
# endif
}

inline 
int32_t io::btoh(int32_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int32_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    SYM_BYTEORDER_CONVERT_32(p0, p1);
    return r;
# endif
}

inline 
int16_t io::btoh(int16_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int32_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    SYM_BYTEORDER_CONVERT_16(p0, p1);
    return r;
# endif
}

inline 
int64_t io::htob(int64_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int64_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    SYM_BYTEORDER_CONVERT_64(p0, p1);
    return r;
# endif
}

inline 
int32_t io::htob(int32_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int32_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    SYM_BYTEORDER_CONVERT_32(p0, p1);
    return r;
# endif
}

inline 
int16_t io::htob(int16_t n) {
# ifdef HOST_BIG_ENDIAN
# error("big-endian not supported")
# else 
    int16_t r;
    unsigned char * p0 = (unsigned char *)&n;
    unsigned char * p1 = (unsigned char *)&r;
    SYM_BYTEORDER_CONVERT_16(p0, p1);
    return r;
# endif
}
