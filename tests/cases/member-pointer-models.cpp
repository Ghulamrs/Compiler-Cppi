// cl sizes a pointer to a member by the class's inheritance model - single,
// multiple, virtual - and cxx1 sized every one as single (review of
// 2026-09-17 against cl, A6). A data member pointer is an int offset, joined
// by a vbtable index under virtual inheritance: 4, 4, 8. A function one is a
// code pointer, joined by an int `this` adjustment under multiple inheritance
// and the vbtable index after that under virtual: 8, 16, 16. The model is
// clang's calculateInheritanceModel, measured against cl: a virtual base
// anywhere is virtual; more than one base at any level of the single-base
// chain, or the first vfptr added over a base without one (Poly), is
// multiple. Itanium's widths are a ptrdiff_t and a pair of them whatever the
// model. With the widths came [conv.mem]: `&Single::f` becomes an
// `int (Multi::*)()` by growing the adjustment by Single's offset, which cxx1
// refused as a type mismatch; the pair is rebuilt in the wider shape where
// the models differ, and a null pointer stays null.
extern "C" int printf(const char *, ...);
#ifdef _WIN32
#define N(itanium, microsoft) microsoft
#else
#define N(itanium, microsoft) itanium
#endif
struct One { char c; };
struct Single { int x; int f(); };
int Single::f() { return x; }
struct Multi : One, Single { int y; int g(); };
int Multi::g() { return y + x; }
struct V { int v; };
struct Virt : virtual V { int z; int h(); };
int Virt::h() { return z * 10 + v; }
struct Plain { int p; };
struct Poly : Plain { virtual int k(); int q; };
int Poly::k() { return q + p; }
struct Deep : Multi { int w; };
struct Holds { char tag; int (Multi::*f)(); int Multi::*d; };

static int bad = 0;
static void check(const char *what, int got, int want) {
    if (got == want) printf("%s ok\n", what);
    else { printf("%s %d, not %d\n", what, got, want); bad++; }
}
int callMulti(Multi &m, int (Multi::*p)()) { return (m.*p)(); }
int readMulti(Multi &m, int Multi::*p) { return m.*p; }

int main() {
    const int p = (int)sizeof(void *);
    check("Single data", (int)sizeof(int Single::*), N(p, 4));
    check("Single fn", (int)sizeof(int (Single::*)()), N(2 * p, 8));
    check("Multi data", (int)sizeof(int Multi::*), N(p, 4));
    check("Multi fn", (int)sizeof(int (Multi::*)()), N(2 * p, 16));
    check("Virt data", (int)sizeof(int Virt::*), N(p, 8));
    check("Virt fn", (int)sizeof(int (Virt::*)()), N(2 * p, 16));
    check("Poly data", (int)sizeof(int Poly::*), N(p, 4));
    check("Poly fn", (int)sizeof(int (Poly::*)()), N(2 * p, 16));
    check("Deep fn", (int)sizeof(int (Deep::*)()), N(2 * p, 16));
    check("sizeof Holds", (int)sizeof(Holds), N(4 * p, 32));
    Multi m; m.c = 1; m.x = 7; m.y = 9;
    int (Multi::*pg)() = &Multi::g;
    int (Multi::*pf)() = &Single::f;          // Single is at 4: the adjustment
    int Multi::*py = &Multi::y;
    int Multi::*px = &Single::x;
    check("own fn", callMulti(m, pg), 16);
    check("base fn", callMulti(m, pf), 7);
    check("own data", readMulti(m, py), 9);
    check("base data", readMulti(m, px), 7);
    Deep d; d.c = 2; d.x = 20; d.y = 30; d.w = 40;
    int (Deep::*dg)() = &Multi::g;
    int (Deep::*df)() = &Single::f;
    int Deep::*dx = &Single::x;
    check("deep own", (d.*dg)(), 50);
    check("deep base", (d.*df)(), 20);
    check("deep data", d.*dx, 20);
    Holds h; h.tag = 't'; h.f = &Single::f; h.d = &Single::x;
    check("held fn", (m.*h.f)(), 7);
    check("held data", m.*h.d, 7);
    Virt o; o.v = 3; o.z = 4;
    int (Virt::*ph)() = &Virt::h;
    int Virt::*pz = &Virt::z;
    check("virt fn", (o.*ph)(), 43);
    check("virt data", o.*pz, 4);
    Poly y; y.p = 5; y.q = 6;
    int (Poly::*pk)() = &Poly::k;
    int Poly::*pp = &Plain::p;
    check("poly fn", (y.*pk)(), 11);
    check("poly data", y.*pp, 5);
    int Single::*nd = nullptr;
    int Multi::*ndm = nd;
    check("null data stays null", ndm == nullptr, 1);
    printf("%d wrong\n", bad);
    return 0;
}
