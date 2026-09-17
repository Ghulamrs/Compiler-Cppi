// **[expr.const]'s floating lane.** `constexpr double d = 1.5;`, a
// static_assert over one, an array length cast from one, and a constexpr
// floating local: fold() answers in integers and refused every one of them
// as "not a constant expression" - registered in tests/open on 2026-09-09.
// The declaration paths store a const floating object through foldDouble now,
// local as well as global, and fold() hands a floating operand to it where an
// integer comes out - a cast, a comparison, `!`, `&&` - so the assertion and
// the array bound read it back. A floating result stays foldDouble's own.
extern "C" int printf(const char *, ...);
constexpr double d = 1.5;
constexpr float f = 2.5f;
constexpr double e = d * 2 + f;
static_assert(d > 1.0, "d");
static_assert(e == 5.5, "e");
static_assert(!(d < 1.0), "not");
static_assert(d > 1.0 && f < 3.0, "and");
int a[(int)e];
int main() {
    constexpr double local = e / 2;
    static_assert(local == 2.75, "local");
    const int n = (int)(local * 4);
    int b[n];
    b[n - 1] = 3;
    a[(int)e - 1] = 4;
    printf("%d %d %d %d\n", (int)sizeof a / (int)sizeof a[0], n, b[n - 1], a[4]);
    return 0;
}
