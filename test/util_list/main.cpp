# include <sym/utilities/list.h>
# include <assert.h>

namespace su = sym::util;

struct Data
{
        int a;
        int b;
        
        bool operator==(const Data & d) const {
            return a == d.a && b == d.b;
        }
};

int main(int argc, char **argv)
{
    su::LinkedList<Data> lst;
    assert(lst.size() == 0);
    
    Data d1 {1, 2}, d2{3, 4}, d3 {5, 6}, d4{7, 8};;
    lst.bpush(d1);
    assert(lst.size() == 1);
    lst.fpush(d2);
    assert(lst.size() == 2);
    
    lst.fpush(d3);
    lst.bpush(d4);
    assert(lst.size() == 4);
    
    
    auto & a1 = lst.front();
    assert( a1 == d3 );
    
    auto & a2 = lst.back();
    assert( a2 == d4);
    
    su::LinkedList<Data>::Iterator it = lst.begin();
    assert(*it == d3);
    ++it;
    assert(*it == d2);
    ++it;
    assert(*it == d1);
    ++it;
    assert(*it == d4);
    ++it;
    assert(it == lst.end());
    return 0;;
}