# pragma once
# include <sym/symdef.h>
# include <stdlib.h>
# include <assert.h>

BEGIN_SYM_NAMESPACE

namespace util 
{

    /// 默认内存分配器
    template<class T>
    class Allocator
    {
    public:
        typedef T   ValueType;
        typedef T * Pointer;
        typedef T & Reference;
        typedef const T * ConstPointer;
        typedef const T & ConstReference;

    public:
        T * allocate(size_t n) 
        {
            void * p = malloc(sizeof(T) * n);
            assert(p);
            return (T*)p;
        }

        void deallocate(T * p, size_t n)
        {
            free(p);
        }

        void construct(T * p, size_t n)
        {
            for ( int i = 0; i < n; ++i ) {
                new (p + i) T();
            }
        }

        void construct(T * p, size_t n, const T & val) 
        {
            for ( int i = 0; i < n; ++i ) {
                new (p + i) T(val);
            }
        }

        void construct(T * p, size_t n, const T * const vals)
        {
            for ( int i = 0; i < n; ++i ) {
                new (p + i) T(vals[i]);
            }
        }

        void destroy(T * p, size_t n) 
        {
            for ( int i = 0; i < n; ++i ) {
                p[i].~T();
            }
        }
    }; // end class Allocator

    /// void类型内存分配器。
    template<>
    class Allocator<void>
    {
    public:
        typedef void   ValueType;
        typedef void * Pointer;
        typedef const void * ConstPointer;
    
    public:
        void * allocate(size_t n) 
        {
            void * p = malloc(n);
            assert(p);
            return p;
        }

        void deallocate(void * p, size_t n) { free(p); }

    }; // end class Allocator<void>


} // end namespace util

END_SYM_NAMESPACE