#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__)
void f(void) { LOG("%d %d\n", 1, 2); }
