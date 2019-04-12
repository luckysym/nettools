#pragma once

#include <stdlib.h>
#include <assert.h>

/// 包含基本数据结构和算法类操作。
namespace alg {

    const int c_padding_size = 8;   ///< 字节对齐常量。

    /// 基本数组模板模板
    template<class T>
    struct basic_array {
        T *   values;
        size_t size;
    }; // end struct basic_array

    /// 基本双链节点模板。
    template <class T>
    struct basic_dlink_node {
        struct basic_dlink_node<T> *next;   ///< 后继节点指针。
        struct basic_dlink_node<T> *prev;   ///< 前驱节点指针。
        T  value;                           ///< 数据对象。
    }; // struct baskc_link_node

    /// 基本双链表模板
    template<class T>
    struct basic_dlink_list {
        basic_dlink_node<T> * front;  ///< 链表首节点，空链表则是null。
        basic_dlink_node<T> * back;   ///< 链表尾节点，空链表则是null。
        size_t                size;   ///< 链表中的节点数。
    }; // end struct basic_dlink_list

    /// 初始化分配一个数组。数组必须是新init，或者已经被free，否则可能导致内存泄漏。
    template<class T>
    basic_array<T> * array_alloc(basic_array<T> * arr, size_t size, const T * init = nullptr);

    /// 重新分配一个数组。数组必须已经执行过alloc, 或者已经被free，否则可能导致越界访问。
    template<class T>
    basic_array<T> * array_realloc(basic_array<T> * arr, size_t size, const T * init = nullptr);
    
    /// 释放一个数组，该数组必须是已经alloc，realloc或者free的。
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

} // end namespace alg, for decl

namespace alg 
{

template<class T>
basic_array<T> * array_alloc(alg::basic_array<T> * arr, size_t size, const T * init)
{
    arr->values = (T*)malloc(sizeof(T) * size);
    assert(arr->values);
    arr->size = size;

    if ( init ) {
        for (size_t i = 0 ; i < size; ++i) arr->values[i] = *init;
    }
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
basic_array<T> * array_realloc(alg::basic_array<T> * arr, size_t size, const T *init)
{
    if ( arr->size < size ) {
        arr->values = (T*)::realloc(arr->values, sizeof(T) * size);
        assert(arr->values);

        if ( init ) {
            for (size_t i = arr->size ; i < size; ++i) arr->values[i] = *init;
        }
    }
    arr->size = size;
    return arr;
}


template<class T>
basic_dlink_list<T> * dlinklist_init(basic_dlink_list<T> * lst)
{
    lst->front = lst->back = nullptr;
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
            if ( lst->size == 0 ) lst->front = lst->back = nullptr;
            break;
        }
        p = p->next;
    }
    return node;
}

template<class T>
basic_dlink_node<T> * dlinklist_push_back(basic_dlink_list<T> * lst, basic_dlink_node<T> *node)
{
    node->prev = lst->back;
    node->next = nullptr;
    if ( lst->back ) {
        lst->back->next = node;
        lst->back = node;
    }
    else {
        lst->back  = node; 
        lst->front = node;
    }
    ++lst->size;

    return node;
}

template<class T>
basic_dlink_node<T> * dlinklist_pop_front(basic_dlink_list<T> * lst)
{
    auto p = lst->front;
    if ( p ) {
        lst->front = lst->front->next;
        --lst->size;

        if ( lst->size == 0 ) lst->front = lst->back = nullptr;
        else lst->front->prev = nullptr;
        
        p->prev = nullptr;
        p->next = nullptr;

    }
    return p;
}




} // end namespace alg, for impl