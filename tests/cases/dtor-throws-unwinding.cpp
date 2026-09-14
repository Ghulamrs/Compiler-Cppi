// [except.terminate]: a destructor that exits with an exception while another
// is unwinding calls std::terminate. That is a region over the cleanup pad's
// own code in the function's table: on the C6000 a catch-and-terminate scope
// the way cl6x writes one, on the Itanium targets a catch-all row whose pad
// calls std::terminate as clang's does. The throw from ~Loud never reaches the
// handler below and nothing after it runs: the output stops at "~Loud ". stderr
// is closed first, since what each runtime says as it terminates is its own.
extern "C" int printf(const char *, ...);
extern "C" int fflush(void *);
extern "C" int close(int);

struct Loud { ~Loud() noexcept(false) { printf("~Loud "); fflush(0); throw 2; } };

static void inner() { Loud l; throw 1; }

int main() {
    printf("before ");
    close(2);
    try { inner(); } catch (int e) { printf("caught %d - and should not have\n", e); }
    printf("returned - and should not have\n");
    return 0;
}
