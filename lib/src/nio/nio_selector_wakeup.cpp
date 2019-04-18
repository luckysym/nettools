#include <sym/nio.h>

namespace nio 
{

bool selector_wakeup(selector_t* sel, err::error_t *err)
{
    int64_t n = 1;
    int r = write(sel->evfd, &n, sizeof(n));
    assert(r > 0 );
    return true;
}
} // end namespace nio 