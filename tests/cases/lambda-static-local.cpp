// **A static local is named by its function's mangled symbol.** [stmt.dcl]/4
// lets any function body declare one, and until 2026-09-17 it was named by
// the function's bare name: `operator().n` for a lambda and `~S.d` for a
// destructor, labels no assembler takes, and one `f.n` for S::f, T::f, f(int)
// and f(double) alike, "symbol already defined". Registered as
// lambda-static-local, the shape that found it; the others are here beside
// it, with a static local that has a constructor and destructor, which goes
// through the guarded path.
extern "C" int printf(const char *, ...);
struct Loud { int v; Loud(int n) : v(n) { printf("+L%d ", n); } ~Loud() { printf("-L%d ", v); } };
struct S {
    int v;
    S() { static int n = 0; v = ++n; }
    ~S() { static int d = 0; printf("~%d ", ++d); }
    int f() { static int n = 0; return ++n; }
    int g() { static Loud l(7); return l.v + f(); }
};
struct T { int f() { static int n = 10; return ++n; } };
namespace a { int g() { static int n = 100; return ++n; } }
namespace b { int g() { static int n = 200; return ++n; } }
int f(int) { static int n = 0; return ++n; }
int f(double) { static int n = 50; return ++n; }
// Each call is sequenced into a local first: the order a call's arguments
// are evaluated in is unspecified, and the two hosts differ.
int main() {
    auto lam = [](int k) { static int n = 0; return ++n + k; };
    { S x, y; printf("%d %d ", x.v, y.v); }
    S s; T t;
    const int f1 = s.f(), f2 = s.f(), f3 = t.f(), g1 = a::g(), g2 = b::g(), g3 = b::g();
    printf("%d %d %d %d %d %d ", f1, f2, f3, g1, g2, g3);
    const int o1 = f(1), o2 = f(1.0), o3 = f(2);
    printf("%d %d %d ", o1, o2, o3);
    const int l1 = lam(1), l2 = lam(1), h1 = s.g(), h2 = s.g();
    printf("%d %d %d %d\n", l1, l2, h1, h2);
    return 0;
}
