#pragma once 

# include <sym/symdef.h>
# include <sym/utilities/allocator.h>
# include <initializer_list>
# include <algorithm>
BEGIN_SYM_NAMESPACE

namespace util 
{
    class BasicLinkedList 
    {
    public:
        struct Node {
            struct Node * prev;
            struct Node * next;
        };

    private:
        size_t  m_size;
        Node    m_stub;
        
    public:
        BasicLinkedList() : m_size(0){ m_stub.prev = m_stub.next = &m_stub; }

        BasicLinkedList(const BasicLinkedList & other) 
            : m_size(other.m_size), m_stub(other.m_stub) {} 

        BasicLinkedList(BasicLinkedList && other) 
            : m_size(other.m_size), m_stub(other.m_stub) 
        { other.m_stub.next = other.m_stub.prev = nullptr; other.m_size = 0; }
        
        ~BasicLinkedList() { m_stub.next = m_stub.prev = nullptr; m_size = 0; }

        BasicLinkedList & operator=(const BasicLinkedList & other) {
            if ( this != &other ) {
                m_size = other.m_size;
                m_stub = other.m_stub;
            }
            return *this;
        }

        BasicLinkedList & operator=(BasicLinkedList && other) {
            if ( this != &other ) {
                m_size = other.m_size;
                m_size = other.m_size;
                m_stub = other.m_stub;
                other.m_stub.next = other.m_stub.prev = nullptr; other.m_size = 0;
            }
            return *this;
        }

        size_t size() const { return m_size; }

        bool empty() const { return m_stub->next == &m_stub}

        Node * begin() { return m_stub.next; }
        const Node * begin() const { return m_stub.next; }

        Node * end () { return &m_stub; }
        const Node * end() const { return &m_stub; }

        Node * rbegin() { return &m_stub; }
        const Node * rbegin() const { return &m_stub; }

        Node * rend() { return m_stub.prev; }
        const Node * rend() const { return m_stub.prev; }

        void pushBack(Node * node) { 
            node->prev = m_stub.prev; node->next = &m_stub; m_stub.prev = node; 
            ++m_size;
        }

        void pushFront(Node * node) {
            node->prev = &m_stub; node->next = m_stub.next; m_stub.next = node;
            ++m_size;
        }

        void popFront() {
            Node * p = m_stub.next;
            if ( p != &m_stub ) {
                p->next->prev = &m_stub;
                m_stub.next = p->next;
                p->next = p->prev = nullptr;
                --m_size;
            } 
        }

        void popBack() {
            Node * p = m_stub.prev;
            if ( p != &m_stub ) {
                p->prev->next = &m_stub;
                m_stub.prev = p->prev;
                p->next = p->prev = nullptr;
                --m_size;
            }
        }

        void erase(Node * node) {
            if( node != &m_stub ) {
                Node * p0 = node->prev;
                Node * p1 = node->next;
                assert( p0 && p1);

                p0->next = p1;
                p1->prev = p0;
                node->prev = node->next = nullptr;
            }
        }
    }; // end BasicLinkedList

    template<class T, class Alloc>
    class LinkedList
    {
    private:
        BasicLinkedList  m_impl;
        Alloc            m_alloc;

    public:
        class Iterator {
        private:
            BasicLinkedList::Node * m_node;
        };
        class ConstIterator {
        private:
            const BasicLinkedList::Node * m_node;
        };
    public:
        LinkedList() {}
        LinkedList(const LinkedList<T, Alloc>& other);
        LinkedList(LinkedList<T, Alloc>&& other);
        
        LinkedList(const Alloc & alloc) {}
        LinkedList(const LinkedList<T, Alloc>& other, const Alloc & alloc);
        LinkedList(LinkedList<T, Alloc>&& other, const Alloc & alloc);
        ~LinkedList();

        LinkedList<T, Alloc> & operator=(const LinkedList<T, Alloc>& other);
        LinkedList<T, Alloc> & operator=(LinkedList<T, Alloc>& other);

        size_t size() const;
        bool   empty() const;

        Iterator begin();
        ConstIterator begin() const;
        
        Iterator end();
        ConstIterator end() const;

        T & front();
        const T & front() const;

        T & back();
        const T& back() const;

        void pushBack(const T& t);
        void pushFront(const T& t);

        void popFont();
        void popBack();

        Iterator erase(Iterator it);

    }; // end class LinkedList

} // end namespace util

END_SYM_NAMESPACE