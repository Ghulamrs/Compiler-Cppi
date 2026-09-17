// cl review A6: cl sizes a pointer to a member by the class's inheritance
// model - single 4/8, multiple 4/16, virtual 8/16 - and the wider forms
// carry a `this` adjustment the other side may fill: cl's `&Multi::f`, f
// being Single's at offset 4, is {f, 4}. This half applies whatever it is
// handed; the other half builds the pointers. Either half may be the cl one.
#include "member-pointer-models.h"
int Single::f() { return x; }
int Multi::g() { return y + x; }
int Virt::h() { return z * 10 + v; }
int Poly::k() { return q + p; }
int callMulti(Multi &m, int (Multi::*p)()) { return (m.*p)(); }
int readMulti(Multi &m, int Multi::*p) { return m.*p; }
int callVirt(Virt &o, int (Virt::*p)()) { return (o.*p)(); }
int readVirt(Virt &o, int Virt::*p) { return o.*p; }
int callPoly(Poly &o, int (Poly::*p)()) { return (o.*p)(); }
int sizes() {
    printf("%d %d %d %d %d %d %d %d\n",
           (int)sizeof(int Single::*), (int)sizeof(int (Single::*)()),
           (int)sizeof(int Multi::*), (int)sizeof(int (Multi::*)()),
           (int)sizeof(int Virt::*), (int)sizeof(int (Virt::*)()),
           (int)sizeof(int Poly::*), (int)sizeof(int (Poly::*)()));
    return 0;
}
