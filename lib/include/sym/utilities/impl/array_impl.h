# pragma once 

#include <sym/symdef.h>
#include <initializer_list>

BEGIN_SYM_NAMESPACE

namespace util
{
    template<class T, class Alloc>
    struct ArrayImpl
    {
        T          *  m_begin;
        size_t        m_len;
        size_t        m_cap;
        Alloc         m_alloc;

        ArrayImpl() : m_begin(nullptr), m_len(0), m_cap(0) {}
        ArrayImpl(const Alloc & alloc) : m_begin(nullptr),m_len(0), m_cap(0), m_alloc(alloc) {}

        /// 分配内存，但不构造元素
        void allocate(size_t cap) {
            if ( cap > 0 ) {
                m_begin = m_alloc.allocate(cap);
                m_cap  = cap;
                m_len  = 0;
            }
        }

        /// 分配内存并构造元素
        void allocate(const std::initializer_list<T> & lst) {
            size_t size = lst.size();
            if ( size > 0 ) {
                m_begin = m_alloc.allocate(size);
                auto it = lst.begin();
                size_t i = 0;
                for ( ; i < size; ++i, ++it) {
                    m_alloc.construct(m_begin + i, 1, *it);
                }
                m_cap = size;
                m_len = size;
            }
        }

        /// 分配n的空间，构造lst.size()个元素
        void allocate(size_t n, const std::initializer_list<T> & lst) {
            size_t size = n;
            if ( size > 0 ) {
                m_begin = m_alloc.allocate(size);
                auto it = lst.begin();
                size_t i = 0;
                for ( ; i < size && it != lst.end(); ++i, ++it) {
                    m_alloc.construct(m_begin + i, 1, *it);
                }
                m_cap = n;
                m_len = i;
            }
        }

        /// 分配并构造size个元素并设置初始化值
        void allocate(size_t size, const T & value) {
            if ( size > 0 ) 
            {
                m_begin = m_alloc.allocate(size);
                m_alloc.construct(m_begin, size, value);
                m_cap  = size;
                m_len  = size;
            }
        }

        T * detach() {
            T * p = m_begin;
            m_begin = nullptr;
            m_cap = 0;
            m_len = 0;
            return p;
        }

        void destroy() {
            if ( m_begin ) {
                m_alloc.destroy(m_begin, m_len);
                m_alloc.deallocate(m_begin, m_cap);
                m_cap  = 0;
                m_len  = 0;
                m_begin = nullptr;
            }
        }

        void resize(size_t n) {
            if ( n < m_len ) {
                for(size_t i = n; i < m_len; ++i) m_begin[i].~T();
                m_len = n;
            } 
            else if ( n > m_len && n <= m_cap) {
                for(size_t i = m_len; i < n; ++i) new (m_begin + i) T();
                m_len = n;
            }
            else if ( n > m_cap ) {
                m_alloc.reallocate(m_begin, m_cap, n);
                for( size_t i = m_len; i < n; ++i) new (m_begin + i) T();
                m_len = n;
                m_cap = n;
            } 
        }

    }; // end struct ArrayImpl 

} // end namespace util

END_SYM_NAMESPACE