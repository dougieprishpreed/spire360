struct A { A() {} A(A&&) {} }; A make() { return A(); } void f() { A a(make()); }
