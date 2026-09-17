// `#pragma pack(n)` caps the alignment of every member, base and bit-field
// unit of a class defined while it is in force, and with it the class's own
// alignment and the rounding of its size. Read and dropped until 2026-09-18
// (review of 2026-09-17 against cl, A5), so a packed header compiled clean
// and came out at its natural size. The forms are cl's: pack(n) with n 1, 2,
// 4, 8 or 16; pack() back to none; pack(push [, id] [, n]) and pack(pop [,
// id]), the pop returning to the named push or the last one; pack(show)
// says nothing here. A class defined inside a packed region keeps its packed
// layout wherever it is used later, and a class defined outside one holds
// its natural layout as a member of a packed class - it is the member's
// alignment that is capped, not its insides. Every number here was measured
// with cl and with clang, and the two agree.
extern "C" int printf(const char *, ...);
#define OFF(T, m) ((int)((char *)&((T *)64)->m - (char *)64))

#pragma pack(push, 1)
struct P1 { char c; int i; short s; };
struct P1b { char c; double d; };
#pragma pack(pop)
#pragma pack(2)
struct P2 { char c; double d; };
struct P2b { char c; long long l; int i; };
#pragma pack()
struct N { char c; double d; };
#pragma pack(push, outer, 4)
struct P4 { char c; double d; };
#pragma pack(push, inner, 1)
struct P4in { char c; double d; };
#pragma pack(pop, inner)
struct P4again { char c; double d; };
#pragma pack(pop, outer)
struct N2 { char c; double d; };

// A base under pack: the base's members are already laid, only its alignment
// as a subobject is capped.
struct B { int i; char c; };
#pragma pack(1)
struct D : B { char d; int j; };
struct Holds { char c; N n; };
struct Arr { char c; int a[3]; };
#pragma pack()
struct AfterAll { char c; int i; };

static int bad = 0;
static void check(const char *what, int got, int want) {
    if (got == want) printf("%s ok\n", what);
    else { printf("%s %d, not %d\n", what, got, want); bad++; }
}

int main() {
    check("sizeof P1", (int)sizeof(P1), 7);
    check("P1.i", OFF(P1, i), 1);
    check("P1.s", OFF(P1, s), 5);
    check("sizeof P1b", (int)sizeof(P1b), 9);
    check("sizeof P2", (int)sizeof(P2), 10);
    check("P2.d", OFF(P2, d), 2);
    check("sizeof P2b", (int)sizeof(P2b), 14);
    check("P2b.i", OFF(P2b, i), 10);
    check("sizeof N", (int)sizeof(N), 16);
    check("sizeof P4", (int)sizeof(P4), 12);
    check("P4.d", OFF(P4, d), 4);
    check("sizeof P4in", (int)sizeof(P4in), 9);
    check("sizeof P4again", (int)sizeof(P4again), 12);
    check("sizeof N2", (int)sizeof(N2), 16);
    check("sizeof D", (int)sizeof(D), 13);
    check("D.d", OFF(D, d), 8);
    check("D.j", OFF(D, j), 9);
    check("sizeof Holds", (int)sizeof(Holds), 17);
    check("Holds.n", OFF(Holds, n), 1);
    check("sizeof Arr", (int)sizeof(Arr), 13);
    check("Arr.a", OFF(Arr, a), 1);
    check("sizeof AfterAll", (int)sizeof(AfterAll), 8);
    P1 p; p.c = 1; p.i = 0x01020304; p.s = 5;
    printf("%d %d %d %d wrong\n", p.c, p.i, p.s, bad);
    return 0;
}
