#pragma once

# define USE_SYM_NAMESPACE
# define USE_SYM_INLINE 


# ifdef USE_SYM_NAMESPACE
#   define NAMESPACE_SYM  sym
#   define BEGIN_SYM_NAMESPACE namespace NAMESPACE_SYM {
#   define END_SYM_NAMESPACE  }
# else 
#   define NAMESPACE_SYM
#   define BEGIN_SYM_NAMESPACE
#   define END_SYM_NAMESPACE
# endif // end USE_SYM_NAMESPACE 

# ifdef USE_SYM_INLINE
#   define SYM_INLINE inline
# else
#   define SYM_INLINE
# endif // USE_SYM_INLINE


/// 类实例不可复制宏
# define SYM_NONCOPYABLE(cls) \
    cls(const cls & other) = delete; \
    cls & operator=(const cls & other) = delete; \


///
# define SYM_MAKE_MOVE_CONSTRUCTOR()


/// 默认的move操作实现
# define SYM_MOVE_OPERATOR_IMPL() \
    do { \
        if ( this != &other) { \
            if ( m_impl != nullptr ) delete m_impl; \
            m_impl = other.m_impl; \
            other.m_impl = nullptr; \
        } \
    } while (0)

/// 计算size按pad对齐的数值
# define SYM_PADDING(size, pad)  (((size ) + (pad - 1)) & ~(pad-1))

/// 判断val是否在列表lst中
# define SYM_VALUE_IN_LIST(val, begin, endval)    \
    ({  \
        bool found = false; \
        for( auto it = begin; *it != endval; ++it) {    \
            if ( *it == val ) { \
                found = true;   \
                break;  \
            }   \
        }   \
        found;  \
    })


