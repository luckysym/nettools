#pragma once

#include <stdlib.h>
#include <assert.h>

namespace alg {

    const int c_padding_size = 8;   ///< 对齐常量

    template<class T>
    struct basic_array {
        T * values;
        int size;
    }; // end struct basic_array

    template <class T>
    struct basic_dlink_node {
        T  value;
        struct basic_dlink_node<T> *next;
        struct basic_dlink_node<T> *prev;
    }; // struct baskc_link_node

    template<class T>
    struct basic_dlink_list {
        basic_dlink_node<T> * front;
        basic_dlink_node<T> * back;
    }; // end struct basic_dlink_list

    template<class T>
    basic_array<T> * array_alloc(basic_array<T> * arr, int size);

    template<class T>
    basic_array<T> * array_realloc(basic_array<T> * arr, int size);
    
    template<class T>
    void array_free(basic_array<T> * arr);

    template<class T>
    basic_dlink_node<T> * dlinklist_push_back(basic_dlink_list<T> * lst, basic_dlink_node<T> *node); 

    template<class T>
    basic_dlink_node<T> * dlinklist_pop_front(basic_dlink_list<T> * lst);

    template<class T>
    basic_dlink_node<T> * dlinklist_get_front(basic_dlink_list<T> * lst);

    template<class T>
    basic_dlink_list<T> * dlinklist_init(basic_dlink_list<T> * lst);

    template<class T>
    basic_dlink_node<T> * dlinklist_remove(basic_dlink_list<T> * lst, basic_dlink_node<T> * node);

} // end namespace alg

template<class T>
alg::basic_array<T> * alg::array_alloc(alg::basic_array<T> * arr, int size)
{
    arr->values = (T*)malloc(sizeof(T) * size);
    assert(arr->values);
    arr->size = size;
    return arr;
}

template<class T>
void alg::array_free(alg::basic_array<T> * arr) {
    if ( arr->values ) {
        free( arr->values );
        arr->values = nullptr;
    } 
    arr->size = 0;
}

template<class T>
alg::basic_array<T> * alg::array_realloc(alg::basic_array<T> * arr, int size)
{
    if ( arr->size < size ) {
        arr->values = (T*)::realloc(arr->values, sizeof(T) * size);
        assert(arr->values);
    }
    arr->size = size;
    return arr;
}


