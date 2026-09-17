#include "this-return.h"
int viaNew(int v) { NT *p = new NT(v + 1); int r = p->x; delete p; return r; }
int main() {
    NT nt(14);
    printf("nt %d\n", nt.x % 1000);
    int t = takeNT(nt);
    printf("took %d\n", t);
    NT *m = makeNT(21);
    printf("made %d\n", m->x);
    delete m;
    printf("via %d\n", viaNew(30));
    return 0;
}
