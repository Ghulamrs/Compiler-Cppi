// A VM-bound workload for timing compilerpp.exe itself: loops, calls,
// arithmetic, and an array, so the interpreter's dispatch is what is measured.
void print_int(int n);
void print_line();

int collatz(int n) {
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0) { n = n / 2; } else { n = 3 * n + 1; }
        steps = steps + 1;
    }
    return steps;
}

int main() {
    int total = 0;
    for (int i = 1; i < 30000; i = i + 1) {
        total = total + collatz(i);
    }
    print_int(total);
    print_line();
    return 0;
}
