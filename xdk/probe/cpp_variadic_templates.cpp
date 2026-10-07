template <typename... T> int count(T...) { return sizeof...(T); } int n = count(1, 2);
