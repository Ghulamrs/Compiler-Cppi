// [except.handle]/3: if copy-initialising the exception declaration exits by
// throwing, std::terminate is called - the handler is not entered and the new
// exception does not propagate. That is a terminate scope over the copy, the
// one an unwinding pad already gets: on the Itanium targets a catch-all row
// whose pad calls std::terminate, as clang's __clang_call_terminate does, and
// on the C6000 cl6x's catch-and-terminate scope. Registered in tests/open on
// 2026-09-09 with `throw E(1)` - a copy clang elides and cxx1 makes, so the
// copy that threw was the throw's own and the outer handler was the right
// answer for that program. `g` is a static, so its copy is nobody's to elide,
// and the copy that throws is the catch's, second in line. The output stops
// at "copy2 "; stderr is closed first, what a runtime says as it terminates
// being its own.
extern "C" int printf(const char *, ...);
extern "C" int fflush(void *);
extern "C" int close(int);

struct E {
    int v, gen;
    E(int n) : v(n), gen(0) {}
    E(const E &o) : v(o.v), gen(o.gen + 1) {
        printf("copy%d ", gen);
        fflush(0);
        if (gen == 2) throw 9;
    }
    ~E() {}
};
static E g(1);

int main() {
    printf("before ");
    close(2);
    try { try { throw g; } catch (E e) { printf("body - and should not have "); } }
    catch (int k) { printf("outer(%d) - and should not have ", k); }
    printf("done - and should not have\n");
    return 0;
}
