#pragma once 

#include <pthread.h>

/// 多线程相关操作的名称空间。 
namespace mt {
    typedef pthread_mutex_t mutex_t;

    bool mutex_init(mutex_t *m, err::error_t * err);

    bool mutex_free(mutex_t *m, err::error_t * err);

    bool mutex_lock(mutex_t * m, err::error_t * err);

    bool mutex_unlock(mutex_t * m, err::error_t * err);
} // end namespace mt

namespace mt {

inline 
bool mutex_init(mutex_t *m, err::error_t *e)
{
    int r = pthread_mutex_init(m, nullptr);
    if ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_init error, %d, %s", strerror(r));
        }
        return false;
    } 
}

inline 
bool mutex_free(mutex_t *m, err::error_t *e) {
    int r = pthread_mutex_destroy(m);
    if ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_destroy error, %d, %s", strerror(r));
        }
        return false;
    }
}

inline 
bool mutex_lock(mutex_t *m, err::error_t *e ) {
    int r = pthread_mutex_lock(m);
    if  ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_lock error, %d, %s", strerror(r));
        }
        return false;
    }
}

inline 
bool mutex_unlock(mutex_t *m, err::error_t *e ) {
    int r = pthread_mutex_unlock(m);
    if  ( r == 0 ) {
        return true;
    } else {
        if ( e ) {
            err::push_error_info(e, 128, "pthread_mutex_unlock error, %d, %s", strerror(r));
        }
        return false;
    }
}

} // end namespace mt
