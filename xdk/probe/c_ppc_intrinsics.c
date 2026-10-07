#include <ppcintrinsics.h>
unsigned int f(const void* p) { return __loadwordbytereverse(0, p); }
