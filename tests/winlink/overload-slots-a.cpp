// cl review A3: cl groups a class's own overloads of one name at the first
// one's vftable slot, latest first - `g(int) g(double) h() g(char)` occupies
// `g(char) g(double) g(int) h()` - where cxx1 appended each in declaration
// order, so g(int) called across the boundary reached g(double). Either half
// may be the cl one; the Plain class is the control.
#include "overload-slots.h"
Virt::Virt() : base(11) {}
Virt::~Virt() { printf("~Virt %d\n", base); }
int Virt::vf(int k) { return k + base; }
int Virt::g(int k) { return 100 + k; }
int Virt::g(double d) { return 200 + (int)d; }
int Virt::h() { return 300; }
int Virt::g(char c) { return 400 + c; }
Plain::Plain() : p(1) {}
Plain::~Plain() { printf("~Plain\n"); }
int Plain::a() { return 1; }
int Plain::b() { return 2; }
int Plain::c() { return 3; }
int callVirt(Virt *v, int k) {
    int a = v->vf(k);
    int b = v->g(k);
    int c = v->g(k + 0.5);
    int d = v->h();
    int e = v->g('A');
    printf("callVirt %d %d %d %d %d\n", a, b, c, d, e);
    return a + b + c + d + e;
}
Virt *makeVirt() { return new Virt(); }
int callPlain(Plain *p) {
    int r = p->a() * 100 + p->b() * 10 + p->c();
    printf("callPlain %d\n", r);
    return r;
}
