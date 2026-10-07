#include <initializer_list>
int f(std::initializer_list<int> l) { return (int)l.size(); } int n = f({1, 2, 3});
