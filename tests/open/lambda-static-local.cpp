// [stmt.dcl]/4 lets a lambda's body declare a static local like any other
// function's. cxx1 names it after the closure's call operator - `operator().n`
// - and the assembler refuses the label: "unknown token in expression". The
// front end accepts the program; the fault is the symbol a static local takes
// inside a replayed body, where the function's name is `operator()` rather
// than the mangled closure's. Found 2026-09-17 while reading a lambda's whole
// body for its return type.
extern "C" int printf(const char *, ...);
int main() {
    auto a = [](int k) { static int n = 0; return ++n + k; };
    printf("%d %d\n", a(1), a(1));
    return 0;
}
