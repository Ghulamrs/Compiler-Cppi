// Where an empty base goes when it is not the first thing in the object.
//
// **Itanium puts every empty base at 0 unless a subobject of the same type
// is there already**, and gives it no bytes. **cl gives the second of two a
// byte of its own**: `struct A3 : E1, E2 { int x; }` has E2 at 1 and x at 4,
// where Itanium has E2 at 0 and x at 0. The rule, measured with cl and the
// same one clang's MicrosoftRecordLayoutBuilder keeps: a class *ends with* a
// zero-sized subobject when the last base or class-typed member laid down
// did (a scalar after it changes nothing), and *leads with* one when its
// first base did; a base that leads with one is pushed a byte past a base
// that ends with one. Both flags are set on a class that is itself
// zero-sized. So `V : E1 { int n; }` ends with one, `Z4 { NE m; V v; NE k; }`
// does not, and `Y : X, E2` pads before E2 because X = `NE, E1` ends with E1.
// A member of an empty class is placed by size alone, on both ABIs.
//
// cxx1 laid every empty base at the cursor, so on Windows two adjacent
// empties shared an address and cl's `(E2 *)&a3` did not point where
// cxx1's did (review of 2026-09-17 against cl, A4). The numbers below are
// clang's for the Itanium targets and cl's for Windows; each line prints
// `ok` when the object laid here matches.
extern "C" int printf(const char *, ...);
#define OFF(T, m) ((int)((char *)&((T *)64)->m - (char *)64))
#define BASE(T, B) ((int)((char *)(B *)((T *)64) - (char *)64))
#ifdef _WIN32
#define N(itanium, microsoft) microsoft
#else
#define N(itanium, microsoft) itanium
#endif

struct E1 {}; struct E2 {}; struct E3 {};
struct NE { int n; };
struct A1 : E1, E2 {};
struct A2 : E1, E2 { char c; };
struct A3 : E1, E2 { int x; };
struct A4 : E1, E2, E3 { int x; };
struct A5 : NE, E1 { int x; };
struct A6 : E1, NE { int x; };
struct A7 : NE, E1, E2 { int x; };
struct A8 : E1 { E2 e; int x; };
struct A9 : E1, E2 { E3 e; int x; };
struct B1 : E1 { char c; };      struct B2 : B1, E2 { int x; };
struct C1 : E1 {};               struct C2 : C1, E2 { int x; };
struct D2 : E1, E2 { double d; };
struct X : NE, E1 {};            struct Y : X, E2 { int x; };
struct YY : Y, E3 { int y; };
struct Z : NE { E1 e; };         struct W : Z, E2 { int x; };
struct V : E1 { int n; };        struct U : V, E2 { int x; };
struct Z3 { V v; };              struct ZZ : Z3, E2 { int x; };
struct Z4 { NE m; V v; NE k; };  struct Z5 : Z4, E2 { int x; };
struct AR { E1 a[2]; };          struct ARR : AR, E2 { int x; };
struct T : E1, E2 {};            struct S : NE, T { int x; };
struct Q : NE, E1 {};            struct P : Q, T { int x; };
struct CC : E1, C1 { int x; };
struct M : E1 { E2 e; E3 f; int x; };
struct G : E1, NE, E2 { int x; };
struct H : E1, E2 { char c[3]; };
struct K : E1, E2, E3 {};
struct L : NE, E1, E2 {};
struct N2 : E1 { char c; };      struct N3 : N2, E2 { char d; };
struct PV { virtual void f(); }; void PV::f() {}
struct PW : PV, E1, E2 { int x; };
struct EE : E1, E2 {};           struct Q2 : EE, E3 { int x; };

static int bad = 0;
static void check(const char *what, int got, int want) {
    if (got == want) printf("%s ok\n", what);
    else { printf("%s %d, not %d\n", what, got, want); bad++; }
}

int main() {
    check("sizeof A1", (int)sizeof(A1), N(1, 1));
    check("sizeof A2", (int)sizeof(A2), N(1, 2));
    check("A2.c", OFF(A2, c), N(0, 1));
    check("sizeof A3", (int)sizeof(A3), N(4, 8));
    check("A3.x", OFF(A3, x), N(0, 4));
    check("A3 E1", BASE(A3, E1), N(0, 0));
    check("A3 E2", BASE(A3, E2), N(0, 1));
    check("sizeof A4", (int)sizeof(A4), N(4, 8));
    check("A4.x", OFF(A4, x), N(0, 4));
    check("A4 E3", BASE(A4, E3), N(0, 2));
    check("sizeof A5", (int)sizeof(A5), N(8, 8));
    check("A5.x", OFF(A5, x), N(4, 4));
    check("sizeof A6", (int)sizeof(A6), N(8, 8));
    check("A6.x", OFF(A6, x), N(4, 4));
    check("sizeof A7", (int)sizeof(A7), N(8, 12));
    check("A7.x", OFF(A7, x), N(4, 8));
    check("A7 E2", BASE(A7, E2), N(0, 5));
    check("sizeof A8", (int)sizeof(A8), N(8, 8));
    check("A8.x", OFF(A8, x), N(4, 4));
    check("sizeof A9", (int)sizeof(A9), N(8, 8));
    check("A9.x", OFF(A9, x), N(4, 4));
    check("sizeof B2", (int)sizeof(B2), N(8, 8));
    check("B2.x", OFF(B2, x), N(4, 4));
    check("B2 E2", BASE(B2, E2), N(0, 2));
    check("sizeof C2", (int)sizeof(C2), N(4, 8));
    check("C2.x", OFF(C2, x), N(0, 4));
    check("sizeof D2", (int)sizeof(D2), N(8, 16));
    check("D2.d", OFF(D2, d), N(0, 8));
    check("sizeof Y", (int)sizeof(Y), N(8, 12));
    check("Y.x", OFF(Y, x), N(4, 8));
    check("Y E2", BASE(Y, E2), N(0, 5));
    check("sizeof YY", (int)sizeof(YY), N(12, 20));
    check("YY E3", BASE(YY, E3), N(0, 13));
    check("sizeof W", (int)sizeof(W), N(12, 16));
    check("W E2", BASE(W, E2), N(0, 9));
    check("sizeof U", (int)sizeof(U), N(8, 12));
    check("U E2", BASE(U, E2), N(0, 5));
    check("sizeof ZZ", (int)sizeof(ZZ), N(8, 12));
    check("ZZ E2", BASE(ZZ, E2), N(0, 5));
    check("sizeof Z5", (int)sizeof(Z5), N(16, 16));
    check("Z5 E2", BASE(Z5, E2), N(0, 12));
    check("sizeof ARR", (int)sizeof(ARR), N(8, 8));
    check("ARR E2", BASE(ARR, E2), N(0, 3));
    check("sizeof S", (int)sizeof(S), N(8, 12));
    check("S T", BASE(S, T), N(0, 4));
    check("sizeof P", (int)sizeof(P), N(8, 12));
    check("P T", BASE(P, T), N(4, 5));
    check("sizeof CC", (int)sizeof(CC), N(4, 8));
    check("CC C1", BASE(CC, C1), N(1, 1));
    check("M.e", OFF(M, e), N(0, 0));
    check("M.f", OFF(M, f), N(1, 1));
    check("M.x", OFF(M, x), N(4, 4));
    check("sizeof G", (int)sizeof(G), N(8, 8));
    check("G E2", BASE(G, E2), N(0, 4));
    check("sizeof H", (int)sizeof(H), N(3, 4));
    check("H.c", OFF(H, c), N(0, 1));
    check("sizeof K", (int)sizeof(K), N(1, 2));
    check("K E3", BASE(K, E3), N(0, 2));
    check("sizeof L", (int)sizeof(L), N(4, 8));
    check("L E2", BASE(L, E2), N(0, 5));
    check("sizeof N3", (int)sizeof(N3), N(2, 3));
    check("N3.d", OFF(N3, d), N(1, 2));
    const int p = (int)sizeof(void *);            // the C6000's vptr is 4 bytes
    check("sizeof PW", (int)sizeof(PW), N(2 * p, 16));
    check("PW.x", OFF(PW, x), N(p, 12));
    check("PW E2", BASE(PW, E2), N(0, 9));
    check("sizeof Q2", (int)sizeof(Q2), N(4, 8));
    check("Q2 E3", BASE(Q2, E3), N(0, 2));
    printf("%d wrong\n", bad);
    return 0;
}
