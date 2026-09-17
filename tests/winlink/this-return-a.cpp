// cl review A1: a Microsoft constructor returns `this` in RAX, and a caller
// compiled by cl uses it - `new NT(v)` hands back the constructor's RAX as it
// stands. A cxx1 constructor that returned 0 gave that caller a null, and
// takeNT then read through it. Either half may be the cl one.
#include "this-return.h"
NT::NT(int v) : x(v) {}
NT::NT(const NT &o) : x(o.x + 1000) {}
NT::~NT() {}
int takeNT(NT n) { printf("NT %d\n", n.x % 1000); return n.x % 1000; }
NT *makeNT(int v) { return new NT(v); }
