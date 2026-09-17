extern "C" int printf(const char *, ...);
struct Virt {
    virtual ~Virt();
    virtual int vf(int k);
    virtual int g(int k);
    virtual int g(double d);
    virtual int h();
    virtual int g(char c);
    int base;
    Virt();
};
struct Plain { virtual int a(); virtual int b(); virtual int c(); virtual ~Plain(); int p; Plain(); };
int callVirt(Virt *v, int k);
Virt *makeVirt();
int callPlain(Plain *p);
