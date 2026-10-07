#define W2C2_HOST_MEMORY
#include "w2c2_base.h"
U32 f(wasmMemory* m) { return i32_load(m, 4); }
