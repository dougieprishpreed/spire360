#include <new>
struct A { int x; A() : x(1) {} }; void f(void* p) { new (p) A(); }
