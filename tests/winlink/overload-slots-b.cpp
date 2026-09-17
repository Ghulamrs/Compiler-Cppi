#include "overload-slots.h"
struct Der : Virt {
    int extra;
    Der() : extra(50) {}
    int vf(int k) { return k + extra; }
    int g(int k) { return 1000 + k; }
    int g(double d) { return 2000 + (int)d; }
    int g(char c) { return 3000 + c; }
    ~Der() { printf("~Der\n"); }
};
struct DP : Plain { int a() { return 7; } int c() { return 9; } };
int main() {
    Der d;
    int cv = callVirt(&d, 3);
    Virt *v = makeVirt();
    int cv2 = callVirt(v, 4);
    delete v;
    Virt *dv = new Der();
    int cv3 = callVirt(dv, 5);
    delete dv;
    printf("virt %d %d %d\n", cv, cv2, cv3);
    DP dp;
    int cp = callPlain(&dp);
    Virt *mv = makeVirt();
    int g1 = mv->g(1);
    int g2 = mv->g(1.5);
    int g3 = mv->g('B');
    delete mv;
    printf("plain %d %d %d %d\n", cp, g1, g2, g3);
    return 0;
}
