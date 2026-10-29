#include "heap.h"

#if defined(__XSDK__) || defined(__linux__) || defined(__CYGWIN__) || defined(__MSYS__)
#include <malloc.h>
#define SPIRE_MALLINFO 1
#endif

namespace spire {

size_t heap_in_use() {
#if defined(SPIRE_MALLINFO) && defined(__GLIBC__) && (__GLIBC__ > 2 || __GLIBC_MINOR__ >= 33)
    struct mallinfo2 m = mallinfo2(); /* mallinfo's counts are ints there, and deprecated */
    return m.uordblks;
#elif defined(SPIRE_MALLINFO)
    struct mallinfo m = mallinfo();
    return (size_t)m.uordblks;
#else
    return 0;
#endif
}

size_t heap_held() {
#if defined(SPIRE_MALLINFO) && defined(__GLIBC__) && (__GLIBC__ > 2 || __GLIBC_MINOR__ >= 33)
    return mallinfo2().arena;
#elif defined(SPIRE_MALLINFO)
    return (size_t)mallinfo().arena;
#else
    return 0;
#endif
}

} /* namespace spire */
