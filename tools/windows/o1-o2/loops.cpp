// Loop kernels of the shape -O2 unrolls - a condition, a straight body, a
// step, no call or branch inside - timed one by one, so the levels can be
// compared on what unrolling reaches. Built by cxx1 and by cl alike; each
// rep changes what the inner loop reads, or cl would hoist the loop away.
#include <stdio.h>
#include <time.h>

static int a[4096];
static int b[4096];

static long long kernelSum(int reps) {
    long long s = 0;
    for (int r = 0; r < reps; r = r + 1) {
        a[r & 4095] = a[r & 4095] + 1;
        for (int i = 0; i < 4096; i = i + 1) s = s + a[i];
    }
    return s;
}

static long long kernelFill(int reps) {
    long long s = 0;
    for (int r = 0; r < reps; r = r + 1) {
        for (int i = 0; i < 4096; i = i + 1) b[i] = i * r;
        s = s + b[r & 4095];
    }
    return s;
}

static long long kernelWhile(int reps) {
    long long s = 0;
    for (int r = 0; r < reps; r = r + 1) {
        int x = 0, st = 1 + (r & 3);
        while (x < 100000) x = x + st;
        s = s + x;
    }
    return s;
}

static long long kernelDot(int reps) {
    long long s = 0;
    for (int r = 0; r < reps; r = r + 1) {
        long long d = 0;
        b[r & 4095] = r;
        for (int i = 0; i < 4096; i = i + 1) d = d + (long long)a[i] * b[i];
        s = s + d;
    }
    return s;
}

static void report(const char *name, long long value, clock_t from) {
    clock_t to = clock();
    printf("%-6s %8ld ms  checksum %lld\n", name, (long)((to - from) * 1000 / CLOCKS_PER_SEC), value);
}

int main() {
    for (int i = 0; i < 4096; i = i + 1) { a[i] = i % 7; b[i] = i % 5; }
    clock_t t;
    t = clock(); report("sum",   kernelSum(20000),   t);
    t = clock(); report("fill",  kernelFill(20000),  t);
    t = clock(); report("while", kernelWhile(2000),  t);
    t = clock(); report("dot",   kernelDot(20000),   t);
    return 0;
}
