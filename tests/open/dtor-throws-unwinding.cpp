// [except.terminate]: a destructor that exits with an exception while another
// is unwinding calls std::terminate. On the host targets cxx1's tables carry
// no must-not-throw region over a cleanup pad, so the throw from ~Loud
// unwinds like a fresh exception and the handler catches 2; clang stops at
// "~Loud ". The C6000 target has the scope since 2026-09-15 (tests/cases/
// dtor-throws-unwinding.cpp), and this is the host half of the same defect.
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
