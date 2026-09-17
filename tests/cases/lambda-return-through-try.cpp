// **A lambda's return type is deduced from every `return` in its body** -
// [expr.prim.lambda]/4, by the C++14 relaxation clang applies to C++11 too -
// wherever the statement sits: in a `try`, an `if`, a loop, a block, with the
// first deciding, and a body with none is void. Registered in tests/open on
// 2026-09-09: only a `return` at the body's own brace level was seen, so `f`
// below deduced void and its own `return` was then refused. The body is read
// whole now with deducingReturn_ set, each `return` reporting its operand's
// type instead of being checked. A nested lambda's `return` is its own.
extern "C" int printf(const char *, ...);
struct P { int a; };
int main() {
    auto f = [](int k) { try { throw 4; } catch (int b) { return k + b; } };
    auto g = [](int k) { if (k > 0) { return k * 2; } else { return -k; } };
    auto h = [](int k) { for (int i = 0; i < 3; i++) if (i == k) return i; return -1; };
    auto v = [](int k) { if (k) return; printf("v "); };
    auto n = [](int k) {
        auto inner = [](int j) { return j * 1.5; };
        if (k) return inner(k) > 1.0 ? 1 : 0;
        return 2;
    };
    auto s = [](int k) { P p; p.a = k; { return p; } };
    auto w = [](int k) { while (k) { return 'c'; } return 'd'; };
    v(0);
    printf("%d %d %d %d %d %d %c\n", f(10), g(3), h(2), n(5), n(0), s(9).a, w(1));
    return 0;
}
