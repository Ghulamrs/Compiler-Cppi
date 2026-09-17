// [istream.formatted.arithmetic]: `>>` into a number takes the characters a
// number can be made of and no more, so what follows the digits is left for
// the next extraction. The string-backed stream read a whitespace-delimited
// word and sscanf'd it, so `"42abc" >> x >> s` gave s nothing (review of
// 2026-09-17 against cl, A9); a FILE-backed stream had this right through
// fscanf. The same walk fixes `>> char`, which took a whole word for one
// character, and `get(char &)` on a string stream, which read the FILE.
// A failed extraction sets failbit and the value is left as it was, as both
// oracles do for these inputs.
#include <sstream>
#include <iostream>
#include <string>
int main() {
    int x = 0; std::string rest;
    std::istringstream a("  42abc"); a >> x >> rest;
    std::cout << x << " [" << rest << "]" << std::endl;
    int y = 0; std::string t;
    std::istringstream b("7 tail"); b >> y >> t;
    std::cout << y << " [" << t << "]" << std::endl;
    int n = 0; std::string after;
    std::istringstream c("-7x"); c >> n >> after;
    std::cout << n << " [" << after << "]" << std::endl;
    double d = 0; std::string more;
    std::istringstream e("3.5e2rest"); e >> d >> more;
    std::cout << d << " [" << more << "]" << std::endl;
    double f = 0; std::string dots;
    std::istringstream g("1.25.5"); g >> f >> dots;
    std::cout << f << " [" << dots << "]" << std::endl;
    int p = 0; std::istringstream h("+5"); h >> p;
    std::cout << p << " " << (h ? "good" : "bad") << std::endl;
    int q = 9; std::istringstream i("abc"); i >> q;
    std::cout << q << " " << (i ? "good" : "bad") << std::endl;
    char ch = '?'; std::string tail;
    std::istringstream j("  xyz"); j >> ch >> tail;
    std::cout << ch << " [" << tail << "]" << std::endl;
    char k1 = '?', k2 = '?';
    std::istringstream k(" ab"); k.get(k1); k.get(k2);
    std::cout << "[" << k1 << "][" << k2 << "]" << std::endl;
    long long big = 0; std::string unit;
    std::istringstream m("123456789012kg"); m >> big >> unit;
    std::cout << big << " [" << unit << "]" << std::endl;
    return 0;
}
