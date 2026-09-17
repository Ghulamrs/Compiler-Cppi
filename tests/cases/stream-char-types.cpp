// [ostream.inserters.character]: `unsigned char` and `signed char` print as
// characters, the same as `char`. <ostream> had `operator<<(char)` alone, so
// the other two promoted to int and printed their codes - `66 67 68` where
// cl and clang print `B C D` (review of 2026-09-17 against cl, A8). An
// explicit `(int)` is how the code is asked for.
#include <iostream>
int main() {
    unsigned char u = 66;
    signed char s = 67;
    char c = 'E';
    std::cout << u << ' ' << s << ' ' << (unsigned char)68 << ' ' << c << std::endl;
    std::cout << (int)u << ' ' << (int)s << ' ' << +c << std::endl;
    const unsigned char *bytes = (const unsigned char *)"hi";
    std::cout << bytes[0] << bytes[1] << ' ' << (unsigned)bytes[0] << std::endl;
    return 0;
}
