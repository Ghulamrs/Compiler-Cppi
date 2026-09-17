// [class.virtual]/8: an override returns the same type as the function it
// overrides, or a pointer or reference to a class derived from what that one
// points at. The slot was taken on name and parameters alone, so this
// compiled and `p->f()` through an `A *` read XMM0 as an int - 0 (review of
// 2026-09-17 against cl, A24; cl's C2555).
struct A { virtual int f() { return 1; } virtual ~A() {} };
struct B : A { double f() { return 2.5; } };
int main() { B b; A *p = &b; return p->f(); }
