/* Patterns where a favour-space and a favour-speed compiler are expected to
   part: a fixed-size clear and copy, division and multiplication by constants,
   a string length, a counted loop, and several small functions in a row, so
   that alignment padding between them shows. Plain C, so cl6x reads it too. */
#include <string.h>

struct S { int v[16]; };

void clear16(struct S *s) { memset(s, 0, sizeof *s); }
void copy16(struct S *d, const struct S *s) { memcpy(d, s, sizeof *s); }
int div7(int x) { return x / 7; }
unsigned mod10(unsigned x) { return x % 10; }
int mul9(int x) { return x * 9; }
int mul100(int x) { return x * 100; }
unsigned len(const char *p) { return (unsigned)strlen(p); }
int sum(const int *a, int n) { int s = 0, i; for (i = 0; i < n; i++) s += a[i]; return s; }
int sum8(const int *a) { int s = 0, i; for (i = 0; i < 8; i++) s += a[i]; return s; }
int f1(int x) { return x + 1; }
int f2(int x) { return x + 2; }
int f3(int x) { return f1(x) + f2(x); }
int sel(int c, int a, int b) { return c ? a : b; }
int sw(int x) { switch (x) { case 1: return 10; case 2: return 20; case 3: return 30; case 4: return 40; default: return 0; } }
