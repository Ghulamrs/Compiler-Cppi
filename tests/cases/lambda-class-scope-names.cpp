// **A lambda body is in the scope of the member function that wrote it** -
// [expr.prim.lambda]/7 - so the class's static members, enumerators, typedefs
// and nested classes are named bare in it, `this` captured or not. Registered
// in tests/open on 2026-09-09: the body is replayed as the closure's call
// operator, currentClass_ is the closure there, and `k` was "not declared".
// closureScope_ records the class each closure was written in, a lambda in a
// lambda inheriting the outer one's, and the four lookups ask it after the
// closure. Outside any class the same names stay undeclared.
extern "C" int printf(const char *, ...);
struct S {
    static int k;
    enum { E = 7 };
    typedef int Num;
    struct In { int z; In() : z(100) {} };
    static int twice(int v) { return v * 2; }
    int f() {
        auto g = []() { Num n = k + (int)E; In i; return n + i.z + twice(3); };
        auto h = [](int a) {
            auto inner = [a]() { return a + k + twice(E); };
            return inner();
        };
        return g() + h(1000);
    }
    int m() { auto q = [this]() { return k + E; }; return q(); }
};
int S::k = 3;
int main() { S s; printf("%d %d\n", s.f(), s.m()); return 0; }
