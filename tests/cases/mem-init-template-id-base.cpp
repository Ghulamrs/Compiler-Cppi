// **A mem-initialiser naming a template-id base** - `D(int x) : P<int>(x) {}`.
// Registered in tests/open on 2026-09-09: the list read the base as an
// identifier and stopped at the `<` with "expected '('", so a class deriving
// from a specialization could not pass its base an argument. Read as a type
// now, the way the base-clause reads it, qualified or virtual included; a
// template-id that is not a base is refused as neither member nor base.
extern "C" int printf(const char *, ...);
namespace n { template <class T> struct P { T v; P(T x) : v(x) {} }; }
template <class T> struct Q { T q; Q(T x) : q(x) {} };
struct D : n::P<int>, virtual Q<char> {
    int e;
    D(int x) : n::P<int>(x), Q<char>('a'), e(x + 1) {}
};
struct F : Q<int> { int w; F(int x) : w(x), Q<int>(x * 2) {} };
int main() {
    D d(7);
    F f(3);
    printf("%d %c %d %d %d\n", d.v, d.q, d.e, f.q, f.w);
    return 0;
}
