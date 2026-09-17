extern "C" int printf(const char *, ...);
struct NT { int x; NT(int v); NT(const NT &o); ~NT(); };
int takeNT(NT n);
NT *makeNT(int v);
int viaNew(int v);
