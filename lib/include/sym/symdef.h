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



