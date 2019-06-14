#pragma once 

# include <sym/symdef.h>
# include <sym/utilities/allocator.h>
# include <sym/utilities/impl/array_impl.h>

# include <algorithm>
# include <initializer_list>

BEGIN_SYM_NAMESPACE

namespace util 
{

    /// 数组模板
    template<class T, class Alloc = Allocator<T> >
    class Array
    {
    public:
        using ValueType     = T;     ///< 值类型
        using AllocatorType = Alloc; ///< 分配器类型
        
    private:
        ArrayImpl<T, Alloc>  m_impl;

    public:

        Array();
        Array(size_t size);
        Array(size_t size, const ValueType & val);
        Array(ValueType * array, size_t size);
        Array(const Array<T, Alloc> & other);
        Array(Array<T, Alloc> && other);
        Array(std::initializer_list<T> lst);
        
        Array(const AllocatorType & alloc);
        Array(size_t size, const AllocatorType & alloc);
        Array(size_t size, const ValueType & val, const AllocatorType & alloc);
        Array(ValueType * array, size_t size, const AllocatorType & alloc);
        Array(const Array<T, Alloc> & other, const AllocatorType & alloc);
        Array(Array<T, Alloc> && other, const AllocatorType & alloc);
        Array(std::initializer_list<T> lst, const AllocatorType & alloc);
        
        ~Array();

        Array<T, Alloc> & operator=(const Array<T, Alloc> & other);
        Array<T, Alloc> & operator=(Array<T, Alloc> && other);
        const T & operator[](size_t n) const;
        T & operator[](size_t n);

        T * data() { return m_impl.m_begin; }
        const T * data() const { return m_impl.m_begin; }

        size_t size() const { return m_impl.m_size; }
        T * detach() { return m_impl.detach(); }
        Alloc & allocator() { return m_impl.m_alloc; }

    }; // end class Array

} // end namespace util


namespace util {

    template<class T, class Alloc>
    Array<T, Alloc>::Array() {}

    template<class T, class Alloc>
    Array<T, Alloc>::Array(size_t size)
    {
        m_impl.allocate(size);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(size_t size, const T & value)
    {
        m_impl.allocate(size, value);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(ValueType * array, size_t size)
    {
        m_impl.m_begin = array;
        m_impl.m_size  = size;
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(const Array<T, Alloc> & other)
    {
        size_t size = other.size();
        m_impl.allocate(size, other.m_impl.m_begin);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(Array<T, Alloc> && other)
    {
        m_impl.m_size = other.size();
        m_impl.m_begin = other.detach();
        m_impl.m_alloc = std::move(other.allocator());
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(std::initializer_list<T> lst) 
    {
        m_impl->allocate(lst);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(const Alloc & alloc) : m_impl(alloc) {}

    template<class T, class Alloc>
    Array<T, Alloc>::Array(size_t size, const Alloc & alloc)
        : m_impl(alloc)
    {
        m_impl.allocate(size);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(size_t size, const T & value, const Alloc & alloc)
        : m_impl(alloc)
    {
        m_impl.allocate(size, value);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(ValueType * array, size_t size, const Alloc & alloc)
        : m_impl(alloc)
    {
        m_impl.m_begin = array;
        m_impl.m_size  = size;
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(const Array<T, Alloc> & other, const Alloc & alloc)
        : m_impl(alloc)
    {
        size_t size = other.size();
        m_impl.allocate(size, other.m_impl.m_begin);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(Array<T, Alloc> && other, const Alloc & alloc)
        : m_impl(alloc)
    {
        m_impl.m_size = other.size();
        m_impl.m_begin = other.detach();
        m_impl.m_alloc = std::move(other.allocator());
    }

    template<class T, class Alloc>
    Array<T, Alloc>::Array(std::initializer_list<T> lst, const Alloc & alloc) 
        : m_impl(alloc)
    {
        m_impl->allocate(lst);
    }

    template<class T, class Alloc>
    Array<T, Alloc>::~Array() 
    {
        m_impl.destroy();
    }

    template<class T, class Alloc>
    Array<T, Alloc> & Array<T, Alloc>::operator=(const Array<T, Alloc> & other)
    {
        if ( this != &other) {
            m_impl.destroy();
            m_impl.allocate(other.m_impl.m_size, other.m_impl.m_begin);
        }
        return *this;
    }
        
    template<class T, class Alloc>
    Array<T, Alloc> & Array<T, Alloc>::operator=(Array<T, Alloc> && other)
    {
        if ( this != &other) {
            m_impl.destroy();
            m_impl.m_size = other.m_impl.m_size;
            m_impl.m_begin = other.detach();
            m_impl.m_alloc = std::move(other.m_impl.m_alloc);
        }
        return *this;
    }

    template<class T, class Alloc>
    const T & Array<T, Alloc>::operator[](size_t n) const
    {
        assert( n <= m_impl.m_size);
        return m_impl.m_begin[n];
    }

    template<class T, class Alloc>
    T & Array<T, Alloc>::operator[](size_t n)
    {
        assert( n <= m_impl.m_size);
        return m_impl.m_begin[n];
    }
    
} // end namespace util


END_SYM_NAMESPACE