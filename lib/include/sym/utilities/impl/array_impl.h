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
        size_t        m_size;
        Alloc         m_alloc;

        ArrayImpl() : m_begin(nullptr), m_size(0) {}
        ArrayImpl(const Alloc & alloc) : m_begin(nullptr), m_size(0), m_alloc(alloc) {}

        void allocate(size_t size) {
            if ( size > 0 ) {
                m_begin = m_alloc.allocate(size);
                m_alloc.construct(m_begin, size);
                m_size  = size;
            }
        }

        void allocate(const std::initializer_list<T> & lst) {
            size_t size = lst.size();
            if ( size > 0 ) {
                m_begin = m_alloc.allocate(size);
                auto it = lst.begin();
                size_t i = 0;
                for ( ; i < size; ++i, ++it) {
                    m_alloc.construct(m_begin + i, 1, *it);
                }
                m_size  = size;
            }
        }

        void allocate(size_t size, const T & value) {
            if ( size > 0 ) {
                m_begin = m_alloc.allocate(size);
                m_alloc.construct(m_begin, size, value);
                m_size  = size;
            }
        }

        void allocate(size_t size, const T * const values) {
            if ( size > 0 ) {
                m_begin = m_alloc.allocate(size);
                m_alloc.construct(m_begin, size, values);
                m_size  = size;
            }
        }

        T * detach() {
            T * p = m_begin;
            m_begin = nullptr;
            m_size = 0;
            return p;
        }

        void destroy() {
            if ( m_begin ) {
                m_alloc.destroy(m_begin, m_size);
                m_alloc.deallocate(m_begin, m_size);
                m_size = 0;
                m_begin = nullptr;
            }
        }

    }; // end struct ArrayImpl 

} // end namespace util

END_SYM_NAMESPACE