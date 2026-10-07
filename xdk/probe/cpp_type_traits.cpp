#include <type_traits>
static_assert(std::is_pod<int>::value, "pod");
