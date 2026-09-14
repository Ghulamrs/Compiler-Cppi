// [except.terminate]: a destructor that exits with an exception while another
// is unwinding calls std::terminate. On the C6000 that is a scope in the
// function's exception table over the cleanup pad's own code - catch anything,
// and terminate - the way cl6x writes one; the throw from ~Loud never reaches
// the handler below, and nothing after it runs. The output stops at "~Loud ".
extern "C" int printf(const char *, ...);
extern "C" int fflush(void *);

struct Loud { ~Loud() noexcept(false) { printf("~Loud "); fflush(0); throw 2; } };

static void inner() { Loud l; throw 1; }

int main() {
    printf("before ");
    try { inner(); } catch (int e) { printf("caught %d - and should not have\n", e); }
    printf("returned - and should not have\n");
    return 0;
}
