// The covariant half of [class.virtual]/8: `D *clone()` over `virtual B *
// clone()` is an override when D derives from B, and a call through the
// base gets a `B *`. With D's B subobject at 0 no thunk is needed and it is
// accepted; a B at an offset would need one that moves the result, which is
// refused by name. A reference is the same rule.
extern "C" int printf(const char *, ...);
struct B { int b; B() : b(1) {} virtual B *self() { return this; } virtual const B &ref() const { return *this; } virtual ~B() {} };
struct D : B { int d; D() : d(2) {} D *self() { return this; } const D &ref() const { return *this; } };
int main() {
    D dd; B *p = &dd;
    B *s = p->self();
    const B &r = p->ref();
    D *direct = dd.self();
    printf("%d %d %d %d\n", s->b, r.b, direct->d, (int)(s == p));
    return 0;
}
