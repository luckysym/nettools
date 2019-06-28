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

        template <class T>
        struct ValueNode : Node 
        {
            T  value;

            ValueNode() {}
            ValueNode(const T & t) : value(t) {}
            ValueNode(T && t) : value(std::move(t)) {}
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

        bool empty() const { return m_stub.next == &m_stub; }

        Node * begin() { return m_stub.next; }
        const Node * begin() const { return m_stub.next; }

        Node * end () { return &m_stub; }
        const Node * end() const { return &m_stub; }

        Node * rbegin() { return m_stub.prev; }
        const Node * rbegin() const { return m_stub.prev; }

        Node * rend() { return &m_stub; }
        const Node * rend() const { return &m_stub; }

        void bpush(Node * node) { 
            node->prev = m_stub.prev; node->next = &m_stub; 
            m_stub.prev->next = node;
            m_stub.prev = node; 
            ++m_size;
        }

        void fpush(Node * node) {
            node->prev = &m_stub; node->next = m_stub.next; 
            m_stub.next->prev = node;
            m_stub.next = node;
            ++m_size;
        }

        void fpop() {
            Node * p = m_stub.next;
            if ( p != &m_stub ) {
                p->next->prev = &m_stub;
                m_stub.next = p->next;
                p->next = p->prev = nullptr;
                --m_size;
            } 
        }

        void bpop() {
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

    template<class T>
    class LinkIterator
    {
    public:
        typedef BasicLinkedList::ValueNode<T> NodeType;
    private:
        mutable NodeType * m_pnode;
    public:
        LinkIterator() : m_pnode(nullptr) {}
        LinkIterator(NodeType * p) : m_pnode(p) {}
        ~LinkIterator() { m_pnode == nullptr; }

        bool operator==(const LinkIterator & it) const { return m_pnode == it.m_pnode; }
        LinkIterator<T>& operator++() { m_pnode = (NodeType*)m_pnode->next; return *this; }
        LinkIterator<T> operator++(int) { auto p = m_pnode; m_pnode = (NodeType*)m_pnode->next; return LinkIterator(p); }

        T & operator*() { return m_pnode->value; }
        const T & operator*() const { return m_pnode->value; }
    }; // end class LinkIterator

    template<class T>
    class ConstLinkIterator
    {
    public:
        typedef BasicLinkedList::ValueNode<T> NodeType;
    private:
        mutable const NodeType * m_pnode;
    public:
        ConstLinkIterator() : m_pnode(nullptr) {}
        ConstLinkIterator(const NodeType * p) : m_pnode(p) {}
        ~ConstLinkIterator() { m_pnode == nullptr; }

        bool operator==(const ConstLinkIterator & it) const { return m_pnode == it.m_pnode; }
        ConstLinkIterator& operator++() { m_pnode = m_pnode->next; return *this; }
        ConstLinkIterator operator++(int) { auto p = m_pnode; m_pnode= m_pnode->next; return ConstLinkIterator(p); }

    }; // end class ConstLinkIterator

    template<class T, class Alloc = Allocator<T> >
    class LinkedList
    {
    public:
        typedef T       ValueType;
        typedef Alloc   AllocType;
        typedef BasicLinkedList::ValueNode<T> NodeType;
        
        typedef LinkIterator<T> Iterator;
        typedef ConstLinkIterator<T> ConstIterator;

    private:
        BasicLinkedList m_list;
        typename AllocType::template Rebind<NodeType>::Other m_alloc;

    public:
        LinkedList();
        LinkedList(const LinkedList<T, Alloc>& other);
        LinkedList(LinkedList<T, Alloc>&& other);
        
        LinkedList(const Alloc & alloc);
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

        void fpush(const T& t);
        void bpush(const T& t);

        void fpop();
        void bpop();

        Iterator erase(Iterator it);
        void clear();
    }; // end class LinkedList

} // end namespace util

END_SYM_NAMESPACE

# include <sym/utilities/impl/list_impl.h>