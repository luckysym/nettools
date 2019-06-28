# pragma once 

# include <sym/symdef.h>
# include <initializer_list>
# include <sym/utilities/list.h>

BEGIN_SYM_NAMESPACE

namespace util
{
    template<class T, class Alloc>
    LinkedList<T, Alloc>::LinkedList() {}

    template<class T, class Alloc>
    LinkedList<T, Alloc>::LinkedList(const LinkedList<T, Alloc>& other)
    {
        for ( auto it = other.begin(); it != other.end(); ++it) {
            NodeType * node = m_alloc.allocate();
            m_alloc.construct(&node->value, *it);
            m_list.bpush(node);
        }
    }

    template<class T, class Alloc>
    LinkedList<T, Alloc>::LinkedList(LinkedList<T, Alloc>&& other)
    {
        m_list = std::move(other.m_list);
        m_alloc = std::move(other.m_alloc);
    }

    template<class T, class Alloc>
    LinkedList<T, Alloc>::~LinkedList() 
    {
        while ( !m_list.empty() ) {
            NodeType * pn = (NodeType *)m_list.begin();
            m_alloc.destroy(pn);
            m_alloc.deallocate(pn);
            m_list.fpop();
        }
    }

    template<class T, class Alloc>
    size_t LinkedList<T, Alloc>::size() const 
    {
        return m_list.size();
    }

    template<class T, class Alloc>
    T & LinkedList<T, Alloc>::front() 
    {
        NodeType * p = (NodeType *)m_list.begin();
        return p->value;
    }

    template<class T, class Alloc>
    const T & LinkedList<T, Alloc>::front() const
    {
        const NodeType * p = (const NodeType *)m_list.begin();
        return p->value;
    }

    template<class T, class Alloc>
    T & LinkedList<T, Alloc>::back() 
    {
        NodeType * p = (NodeType *)m_list.rbegin();
        return p->value;
    }

    template<class T, class Alloc>
    const T & LinkedList<T, Alloc>::back() const
    {
        const NodeType * p = (const NodeType *)m_list.rbegin();
        return p->value;
    }

    template<class T, class Alloc>
    void LinkedList<T, Alloc>::bpush(const T & value)
    {
        NodeType * pn = m_alloc.allocate();
        m_alloc.construct(pn, 1, value);
        m_list.bpush(pn);
    }

    template<class T, class Alloc>
    void LinkedList<T, Alloc>::fpush(const T & value)
    {
        NodeType * pn = m_alloc.allocate();
        m_alloc.construct(pn, 1, value);
        m_list.fpush(pn);
    }

    template<class T, class Alloc>
    typename LinkedList<T, Alloc>::Iterator LinkedList<T, Alloc>::begin()
    {
        NodeType *pn = (NodeType*)m_list.begin();
        return Iterator(pn);
    }
    
    template<class T, class Alloc>
    typename LinkedList<T, Alloc>::ConstIterator LinkedList<T, Alloc>::begin() const
    {
        const NodeType * pn = (NodeType*)m_list.begin();
        return ConstIterator(pn);
    }
    
    template<class T, class Alloc>
    typename LinkedList<T, Alloc>::Iterator LinkedList<T, Alloc>::end()
    {
        NodeType *pn = (NodeType*)m_list.end();
        return Iterator(pn);
    }
    
    template<class T, class Alloc>
    typename LinkedList<T, Alloc>::ConstIterator LinkedList<T, Alloc>::end() const
    {
        const NodeType * pn = (NodeType*)m_list.end();
        return ConstIterator(pn);
    }
} // end namespace util

END_SYM_NAMESPACE