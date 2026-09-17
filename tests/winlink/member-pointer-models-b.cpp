#include "member-pointer-models.h"
int main() {
    sizes();
    Multi m; m.c = 1; m.x = 7; m.y = 9;
    int (Multi::*pg)() = &Multi::g;
    int (Multi::*pf)() = &Single::f;          // Single is at 4: the adjustment
    int Multi::*py = &Multi::y;
    int Multi::*px = &Single::x;
    printf("%d %d %d %d\n", callMulti(m, pg), callMulti(m, pf), readMulti(m, py), readMulti(m, px));
    Virt o; o.v = 3; o.z = 4;
    int (Virt::*ph)() = &Virt::h;
    int Virt::*pz = &Virt::z;
    printf("%d %d\n", callVirt(o, ph), readVirt(o, pz));
    Poly y; y.p = 5; y.q = 6;
    int (Poly::*pk)() = &Poly::k;
    printf("%d\n", callPoly(y, pk));
    return 0;
}
