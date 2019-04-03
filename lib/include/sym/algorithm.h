#pragma once

#include <stdlib.h>
#include <assert.h>

namespace alg {

    const int c_padding_size = 8;   ///< 对齐常量

    template<class T>
    struct basic_array {
        T *   values;
        size_t size;
    }; // end struct basic_array

    template <class T>
    struct basic_dlink_node {
        struct basic_dlink_node<T> *next;
        struct basic_dlink_node<T> *prev;
        T  value;
    }; // struct baskc_link_node

    template<class T>
    struct basic_dlink_list {
        basic_dlink_node<T> * front;
        basic_dlink_node<T> * back;
        size_t                size;
    }; // end struct basic_dlink_list

    template<class T>
    basic_array<T> * array_alloc(basic_array<T> * arr, size_t size);

    template<class T>
    basic_array<T> * array_realloc(basic_array<T> * arr, size_t size);
    
    template<class T>
    void array_free(basic_array<T> * arr);

    template<class T>
    basic_dlink_list<T> * dlinklist_init(basic_dlink_list<T> * lst);

    template<class T>
    basic_dlink_node<T> * dlinklist_remove(basic_dlink_list<T> * lst, basic_dlink_node<T> * node);

    template<class T>
    basic_dlink_node<T> * dlinklist_push_back(basic_dlink_list<T> * lst, basic_dlink_node<T> *node); 

    template<class T>
    basic_dlink_node<T> * dlinklist_pop_front(basic_dlink_list<T> * lst);

    template<class T>
    basic_dlink_node<T> * dlinklist_get_front(basic_dlink_list<T> * lst);

} // end namespace alg, for decl

namespace alg 
{

template<class T>
basic_array<T> * array_alloc(alg::basic_array<T> * arr, size_t size)
{
    arr->values = (T*)malloc(sizeof(T) * size);
    assert(arr->values);
    arr->size = size;
    return arr;
}

template<class T>
void array_free(basic_array<T> * arr) {
    if ( arr->values ) {
        free( arr->values );
        arr->values = nullptr;
    } 
    arr->size = 0;
}

template<class T>
basic_array<T> * array_realloc(alg::basic_array<T> * arr, size_t size)
{
    if ( arr->size < size ) {
        arr->values = (T*)::realloc(arr->values, sizeof(T) * size);
        assert(arr->values);
    }
    arr->size = size;
    return arr;
}


template<class T>
basic_dlink_list<T> * dlinklist_init(basic_dlink_list<T> * lst)
{
    lst->front = (basic_dlink_node<T>*)lst;
    lst->back  = (basic_dlink_node<T>*)lst;
    lst->size  = 0;
    return lst;
}

template<class T>
basic_dlink_node<T> * dlinklist_remove(basic_dlink_list<T> * lst, basic_dlink_node<T> * node)
{
    auto p = lst->front;
    while ( p ) {
        if ( p == node ) {
            auto p0 = p->prev;
            auto p1 = p->next;
            if ( p0 ) p0->next = p1;
            if ( p1 ) p1->prev = p0;
            --lst->size;
            break;
        }
        p = p->next;
    }
}




} // end namespace alg, for impl