#include "Optimizer.h"

#include <cstring>

IrOp IrOp::from(const Op &o) {
    IrOp r;
    r.kind = o.kind;
    r.text.assign(o.text.p, o.text.n);
    r.disp = o.disp;
    r.hasDisp = o.hasDisp;
    r.uimm = o.uimm;
    r.immNeg = o.immNeg;
    r.immNumeric = o.immNumeric;
    return r;
}

Op IrOp::view() const {
    return { kind, Str(text), disp, hasDisp, uimm, immNeg, immNumeric };
}

bool IrOp::same(const IrOp &o) const {
    return kind == o.kind && text == o.text && disp == o.disp &&
           hasDisp == o.hasDisp && uimm == o.uimm && immNeg == o.immNeg &&
           immNumeric == o.immNumeric;
}

// ---------------------------------------------------------------------------
// The register model.

// **Every table in this file is a constant with a constant initialiser.** Not
// a static local built on first use, which would be a guard - a lock, in
// effect - taken by every thread of the driver's pool on every call.

namespace {

const unsigned kAll = 0xFFFFFFFFu;
enum { kRax = 0, kRbx = 1, kRcx = 2, kRdx = 3, kRsi = 4, kRdi = 5, kRbp = 6, kRsp = 7,
       kGprCount = 16, kXmm0 = 16 };

constexpr unsigned bit(int r) { return 1u << static_cast<unsigned>(r); }

// The four names of each general register, widest first.
constexpr const char *const kGprNames[kGprCount][4] = {
    { "%rax", "%eax",  "%ax",   "%al"   },
    { "%rbx", "%ebx",  "%bx",   "%bl"   },
    { "%rcx", "%ecx",  "%cx",   "%cl"   },
    { "%rdx", "%edx",  "%dx",   "%dl"   },
    { "%rsi", "%esi",  "%si",   "%sil"  },
    { "%rdi", "%edi",  "%di",   "%dil"  },
    { "%rbp", "%ebp",  "%bp",   "%bpl"  },
    { "%rsp", "%esp",  "%sp",   "%spl"  },
    { "%r8",  "%r8d",  "%r8w",  "%r8b"  },
    { "%r9",  "%r9d",  "%r9w",  "%r9b"  },
    { "%r10", "%r10d", "%r10w", "%r10b" },
    { "%r11", "%r11d", "%r11w", "%r11b" },
    { "%r12", "%r12d", "%r12w", "%r12b" },
    { "%r13", "%r13d", "%r13w", "%r13b" },
    { "%r14", "%r14d", "%r14w", "%r14b" },
    { "%r15", "%r15d", "%r15w", "%r15b" },
};

// **Which 64-bit register a name is, and how wide the name is.** The high
// bytes %ah..%dh answer with width 0: they alias their register, so they count
// as a read and a part write of it, but nothing here will rename one.

// An x87 name answers -2 and is ignored; anything else answers -1, and the
// instruction naming it is treated as doing anything at all.
int regLookup(const std::string &name, int &width) {
    width = 0;
    if (name.size() < 3 || name[0] != '%') return -1;
    if (name.compare(0, 4, "%xmm") == 0) {
        int n = 0;
        for (std::size_t i = 4; i < name.size(); i++) {
            if (name[i] < '0' || name[i] > '9') return -1;
            n = n * 10 + (name[i] - '0');
            if (n > 15) return -1;
        }
        if (name.size() == 4) return -1;
        width = 16;
        return kXmm0 + n;
    }
    if (name[1] == 's' && name[2] == 't') return -2;
    const std::size_t n = name.size();
    // r8..r15, with d, w or b for the narrower names.
    if (name[1] == 'r' && name[2] >= '0' && name[2] <= '9') {
        int v = name[2] - '0';
        std::size_t i = 3;
        if (i < n && name[i] >= '0' && name[i] <= '9') v = v * 10 + (name[i++] - '0');
        if (v < 8 || v > 15) return -1;
        if (i == n) { width = 8; return v; }
        if (i + 1 != n) return -1;
        switch (name[i]) {
        case 'd': width = 4; return v;
        case 'w': width = 2; return v;
        case 'b': width = 1; return v;
        default: return -1;
        }
    }
    // The eight legacy registers: a two-letter core, with r or e in front for
    // the wide names, and l behind for the low byte of si, di, bp and sp.
    std::size_t at = 1;
    int w = 2;
    if (name[1] == 'r') { at = 2; w = 8; }
    else if (name[1] == 'e') { at = 2; w = 4; }
    if (at + 2 != n && at + 3 != n) return -1;
    const char c0 = name[at], c1 = name[at + 1];
    int r = -1;
    bool eightBit = false;
    if (c1 == 'x' || c1 == 'l' || c1 == 'h') {
        switch (c0) {
        case 'a': r = kRax; break;
        case 'b': r = kRbx; break;
        case 'c': r = kRcx; break;
        case 'd': r = kRdx; break;
        default: return -1;
        }
        if (c1 != 'x') {
            if (w != 2) return -1;
            eightBit = true;
            w = c1 == 'l' ? 1 : 0;
        }
    } else if (c0 == 's' && c1 == 'i') r = kRsi;
    else if (c0 == 'd' && c1 == 'i') r = kRdi;
    else if (c0 == 'b' && c1 == 'p') r = kRbp;
    else if (c0 == 's' && c1 == 'p') r = kRsp;
    else return -1;
    if (at + 3 == n) {
        // sil, dil, bpl, spl - and nothing else has a third letter.
        if (eightBit || w != 2 || name[at + 2] != 'l' || r < kRsi) return -1;
        w = 1;
    }
    width = w;
    return r;
}

const char *regName(int canon, int width) {
    int w = width == 8 ? 0 : width == 4 ? 1 : width == 2 ? 2 : 3;
    return kGprNames[canon][w];
}

// Whether naming this register at this width costs a REX prefix.
bool needsRex(int canon, int width) {
    if (canon >= 8) return true;
    return width == 1 && canon >= kRsi && canon <= kRsp;
}

// ---------------------------------------------------------------------------
// The instruction model.

enum { kFlagsDef = 1, kFlagsUse = 2 };

struct Mnemonic {
    const char *name;
    unsigned char len;
    IrSem::Class cls;
    unsigned char flags;
};
// The length is part of the entry, so a lookup is a length test and a memcmp.
#define MN(name, cls, flags) { name, sizeof(name) - 1, cls, flags }

// **Every mnemonic the x86-64 walker writes, and what it does.** A mnemonic
// that is not here is Unknown, which reads and writes everything - so a new
// instruction in the walker costs an optimisation, never a miscompile.
constexpr Mnemonic kMnemonics[] = {
    MN("mov", IrSem::Move, 0),       MN("movq", IrSem::Move, 0),
    MN("movl", IrSem::Move, 0),      MN("movw", IrSem::Move, 0),
    MN("movb", IrSem::Move, 0),      MN("movabs", IrSem::Move, 0),
    MN("movzbq", IrSem::Move, 0),    MN("movzbl", IrSem::Move, 0),
    MN("movzwq", IrSem::Move, 0),    MN("movzwl", IrSem::Move, 0),
    MN("movzbw", IrSem::Move, 0),
    MN("movsbq", IrSem::Move, 0),    MN("movswq", IrSem::Move, 0),
    MN("movslq", IrSem::Move, 0),    MN("movsbl", IrSem::Move, 0),
    MN("movswl", IrSem::Move, 0),    MN("movsbw", IrSem::Move, 0),
    MN("lea", IrSem::Move, 0),
    MN("movsd", IrSem::Move, 0),     MN("movss", IrSem::Move, 0),
    MN("movd", IrSem::Move, 0),      MN("movaps", IrSem::Move, 0),
    MN("movapd", IrSem::Move, 0),    MN("movdqa", IrSem::Move, 0),
    MN("movups", IrSem::Move, 0),    MN("movupd", IrSem::Move, 0),
    MN("cvtsi2sdq", IrSem::Move, 0), MN("cvtsi2sdl", IrSem::Move, 0),
    MN("cvtsi2sd", IrSem::Move, 0),  MN("cvtsi2ssq", IrSem::Move, 0),
    MN("cvtsi2ssl", IrSem::Move, 0), MN("cvtsi2ss", IrSem::Move, 0),
    MN("cvttsd2si", IrSem::Move, 0), MN("cvttsd2siq", IrSem::Move, 0),
    MN("cvttsd2sil", IrSem::Move, 0),MN("cvttss2si", IrSem::Move, 0),
    MN("cvttss2siq", IrSem::Move, 0),MN("cvttss2sil", IrSem::Move, 0),
    MN("cvtss2sd", IrSem::Move, 0),  MN("cvtsd2ss", IrSem::Move, 0),
    MN("sqrtsd", IrSem::Move, 0),    MN("sqrtss", IrSem::Move, 0),

    MN("add", IrSem::Rmw, kFlagsDef),  MN("sub", IrSem::Rmw, kFlagsDef),
    MN("and", IrSem::Rmw, kFlagsDef),  MN("or", IrSem::Rmw, kFlagsDef),
    MN("xor", IrSem::Rmw, kFlagsDef),  MN("imul", IrSem::Rmw, kFlagsDef),
    MN("addq", IrSem::Rmw, kFlagsDef), MN("subq", IrSem::Rmw, kFlagsDef),
    MN("andq", IrSem::Rmw, kFlagsDef), MN("orq", IrSem::Rmw, kFlagsDef),
    MN("xorq", IrSem::Rmw, kFlagsDef), MN("imulq", IrSem::Rmw, kFlagsDef),
    MN("addl", IrSem::Rmw, kFlagsDef), MN("subl", IrSem::Rmw, kFlagsDef),
    MN("andl", IrSem::Rmw, kFlagsDef), MN("orl", IrSem::Rmw, kFlagsDef),
    MN("xorl", IrSem::Rmw, kFlagsDef), MN("imull", IrSem::Rmw, kFlagsDef),
    MN("addw", IrSem::Rmw, kFlagsDef), MN("subw", IrSem::Rmw, kFlagsDef),
    MN("andw", IrSem::Rmw, kFlagsDef), MN("orw", IrSem::Rmw, kFlagsDef),
    MN("xorw", IrSem::Rmw, kFlagsDef),
    MN("addb", IrSem::Rmw, kFlagsDef), MN("subb", IrSem::Rmw, kFlagsDef),
    MN("andb", IrSem::Rmw, kFlagsDef), MN("orb", IrSem::Rmw, kFlagsDef),
    MN("xorb", IrSem::Rmw, kFlagsDef),
    // A shift by zero leaves the flags alone, so a shift is neither a
    // writer to stop at nor a reader; the flags stay live through it.
    MN("shl", IrSem::Rmw, 0),  MN("sal", IrSem::Rmw, 0),
    MN("sar", IrSem::Rmw, 0),  MN("shr", IrSem::Rmw, 0),
    MN("shlq", IrSem::Rmw, 0), MN("salq", IrSem::Rmw, 0),
    MN("sarq", IrSem::Rmw, 0), MN("shrq", IrSem::Rmw, 0),
    MN("shll", IrSem::Rmw, 0), MN("sall", IrSem::Rmw, 0),
    MN("sarl", IrSem::Rmw, 0), MN("shrl", IrSem::Rmw, 0),
    MN("shlw", IrSem::Rmw, 0), MN("sarw", IrSem::Rmw, 0),
    MN("shrw", IrSem::Rmw, 0), MN("shlb", IrSem::Rmw, 0),
    MN("sarb", IrSem::Rmw, 0), MN("shrb", IrSem::Rmw, 0),
    MN("addsd", IrSem::Rmw, 0), MN("subsd", IrSem::Rmw, 0),
    MN("mulsd", IrSem::Rmw, 0), MN("divsd", IrSem::Rmw, 0),
    MN("addss", IrSem::Rmw, 0), MN("subss", IrSem::Rmw, 0),
    MN("mulss", IrSem::Rmw, 0), MN("divss", IrSem::Rmw, 0),
    MN("minsd", IrSem::Rmw, 0), MN("maxsd", IrSem::Rmw, 0),
    MN("pxor", IrSem::Rmw, 0),  MN("xorps", IrSem::Rmw, 0),
    MN("xorpd", IrSem::Rmw, 0), MN("andps", IrSem::Rmw, 0),
    MN("andpd", IrSem::Rmw, 0), MN("orps", IrSem::Rmw, 0),
    MN("orpd", IrSem::Rmw, 0),

    MN("cmp", IrSem::Cmp, kFlagsDef),   MN("cmpq", IrSem::Cmp, kFlagsDef),
    MN("cmpl", IrSem::Cmp, kFlagsDef),  MN("cmpw", IrSem::Cmp, kFlagsDef),
    MN("cmpb", IrSem::Cmp, kFlagsDef),  MN("test", IrSem::Cmp, kFlagsDef),
    MN("testq", IrSem::Cmp, kFlagsDef), MN("testl", IrSem::Cmp, kFlagsDef),
    MN("testw", IrSem::Cmp, kFlagsDef), MN("testb", IrSem::Cmp, kFlagsDef),
    MN("ucomisd", IrSem::Cmp, kFlagsDef), MN("ucomiss", IrSem::Cmp, kFlagsDef),
    MN("comisd", IrSem::Cmp, kFlagsDef),  MN("comiss", IrSem::Cmp, kFlagsDef),

    MN("neg", IrSem::Unary, kFlagsDef),  MN("negq", IrSem::Unary, kFlagsDef),
    MN("negl", IrSem::Unary, kFlagsDef), MN("not", IrSem::Unary, 0),
    MN("notq", IrSem::Unary, 0),         MN("notl", IrSem::Unary, 0),
    // inc and dec leave CF, so they are not a writer of all the flags.
    MN("inc", IrSem::Unary, 0),   MN("dec", IrSem::Unary, 0),
    MN("incq", IrSem::Unary, 0),  MN("decq", IrSem::Unary, 0),
    MN("incl", IrSem::Unary, 0),  MN("decl", IrSem::Unary, 0),

    MN("push", IrSem::Push, 0), MN("pushq", IrSem::Push, 0),
    MN("pop", IrSem::Pop, 0),   MN("popq", IrSem::Pop, 0),

    MN("cqo", IrSem::Cqo, 0),   MN("cqto", IrSem::Cqo, 0),
    MN("cdq", IrSem::Cdq, 0),   MN("cltd", IrSem::Cdq, 0),
    MN("cltq", IrSem::Cltq, 0), MN("cdqe", IrSem::Cltq, 0),

    // The flags after a division are undefined, so no reader may depend on
    // what was there before it: it counts as a writer.
    MN("idiv", IrSem::Div, kFlagsDef),  MN("div", IrSem::Div, kFlagsDef),
    MN("idivq", IrSem::Div, kFlagsDef), MN("divq", IrSem::Div, kFlagsDef),
    MN("idivl", IrSem::Div, kFlagsDef), MN("divl", IrSem::Div, kFlagsDef),
    MN("mul", IrSem::Div, kFlagsDef),   MN("mulq", IrSem::Div, kFlagsDef),
    MN("mull", IrSem::Div, kFlagsDef),

    MN("fldt", IrSem::X87, 0),   MN("fldl", IrSem::X87, 0),
    MN("flds", IrSem::X87, 0),   MN("fld", IrSem::X87, 0),
    MN("fld1", IrSem::X87, 0),   MN("fldz", IrSem::X87, 0),
    MN("fstpt", IrSem::X87, 0),  MN("fstpl", IrSem::X87, 0),
    MN("fstps", IrSem::X87, 0),  MN("fstp", IrSem::X87, 0),
    MN("fildq", IrSem::X87, 0),  MN("fildl", IrSem::X87, 0),
    MN("fild", IrSem::X87, 0),   MN("fistpq", IrSem::X87, 0),
    MN("fistpl", IrSem::X87, 0), MN("fisttpq", IrSem::X87, 0),
    MN("fisttpl", IrSem::X87, 0),MN("fldcw", IrSem::X87, 0),
    MN("fnstcw", IrSem::X87, 0), MN("fstcw", IrSem::X87, 0),
    MN("faddp", IrSem::X87, 0),  MN("fsubp", IrSem::X87, 0),
    MN("fsubrp", IrSem::X87, 0), MN("fmulp", IrSem::X87, 0),
    MN("fdivp", IrSem::X87, 0),  MN("fdivrp", IrSem::X87, 0),
    MN("fxch", IrSem::X87, 0),   MN("fchs", IrSem::X87, 0),
    MN("fabs", IrSem::X87, 0),
    MN("fucomip", IrSem::X87, kFlagsDef), MN("fcomip", IrSem::X87, kFlagsDef),
    MN("fucomi", IrSem::X87, kFlagsDef),  MN("fcomi", IrSem::X87, kFlagsDef),

    MN("call", IrSem::Call, 0),
    MN("ret", IrSem::Ret, 0),
    MN("leave", IrSem::Leave, 0),
    MN("nop", IrSem::Nop, 0),
};

const Mnemonic *findMnemonic(const std::string &m) {
    const std::size_t n = m.size();
    for (const Mnemonic &e : kMnemonics)
        if (n == e.len && std::memcmp(m.data(), e.name, n) == 0) return &e;
    return nullptr;
}

bool startsWith(const std::string &s, const char *p) {
    return s.compare(0, std::strlen(p), p) == 0;
}

bool isShift(const std::string &m) {
    return startsWith(m, "sh") || startsWith(m, "sa");
}

bool isMultiply(const std::string &m) {
    return m == "imul" || m == "imulq" || m == "imull" || m == "mul" ||
           m == "mulq" || m == "mull";
}

bool isX87Store(const std::string &m) {
    return startsWith(m, "fst") || startsWith(m, "fist") || startsWith(m, "fnst");
}

// What must be intact when a function returns: the return registers and the
// ones the callee preserves.
constexpr unsigned kRetLive = bit(kRax) | bit(kRdx) | bit(kRbx) | bit(kRbp) | bit(kRsp) |
                          bit(12) | bit(13) | bit(14) | bit(15) |
                          bit(kXmm0) | bit(kXmm0 + 1);

struct Describer {
    IrSem s;
    bool bad = false;

    void read(const IrOp &o, bool fixed) {
        if (o.kind != Op::Reg && o.kind != Op::Mem && o.kind != Op::Ind) return;
        int w;
        int r = regLookup(o.text, w);
        if (r == -2) return;
        if (r < 0) { bad = true; return; }
        s.use |= bit(r);
        if (fixed) s.fixed |= bit(r);
    }

    // `alsoRead`: the class reads the destination before writing it.
    void write(const IrOp &o, bool alsoRead, bool isDst) {
        if (o.kind == Op::Mem) { s.memWrite = true; read(o, false); return; }
        if (o.kind == Op::Rip) { s.memWrite = true; return; }
        if (o.kind != Op::Reg) { bad = true; return; }
        int w;
        int r = regLookup(o.text, w);
        if (r == -2) return;
        if (r < 0) { bad = true; return; }
        if (alsoRead) { s.use |= bit(r); s.fixed |= bit(r); }
        if (w == 8 || w == 4) {
            s.def |= bit(r);
            if (isDst && !alsoRead) { s.dstReg = r; s.dstWidth = w; }
        } else {
            // A part write keeps the rest of the register, so it reads it.
            s.part |= bit(r);
            s.use |= bit(r);
            s.fixed |= bit(r);
        }
    }

    void everything() {
        s = IrSem();
        s.cls = IrSem::Unknown;
        s.use = kAll;
        s.fixed = kAll;
        s.flagsUse = s.flagsDef = true;
        s.memWrite = true;
    }
};

IrSem describe(const IrIns &i) {
    Describer d;
    IrSem &s = d.s;
    const Mnemonic *e = findMnemonic(i.m);
    if (e != nullptr) {
        s.cls = e->cls;
        s.flagsDef = (e->flags & kFlagsDef) != 0;
        s.flagsUse = (e->flags & kFlagsUse) != 0;
    } else if (!i.m.empty() && i.m[0] == 'j') {
        s.cls = IrSem::Jump;
        s.flagsUse = i.m != "jmp";
    } else if (startsWith(i.m, "set")) {
        s.cls = IrSem::Set;
        s.flagsUse = true;
    } else if (startsWith(i.m, "cmov")) {
        s.cls = IrSem::Rmw;
        s.flagsUse = true;
    } else {
        d.everything();
        return s;
    }
    if (s.cls == IrSem::Rmw && i.operands == 1) {
        // imul %r is rax:rdx = rax * r; shl %r is a shift by one.
        s.cls = isMultiply(i.m) ? IrSem::Div : IrSem::Unary;
    }

    switch (s.cls) {
    case IrSem::Move:
        if (i.operands != 2) { d.bad = true; break; }
        d.read(i.a, false);
        d.write(i.b, false, true);
        break;
    case IrSem::Rmw:
        if (i.operands != 2) { d.bad = true; break; }
        d.read(i.a, isShift(i.m) && i.a.kind == Op::Reg);
        d.write(i.b, true, false);
        if (s.flagsUse) {
            // cmov: the destination keeps its value when the condition fails.
            s.part |= s.def;
            s.def = 0;
        }
        break;
    case IrSem::Cmp:
        if (i.operands != 2) { d.bad = true; break; }
        d.read(i.a, false);
        d.read(i.b, false);
        break;
    case IrSem::Set:
        if (i.operands != 1) { d.bad = true; break; }
        d.write(i.a, true, false);
        break;
    case IrSem::Unary:
        if (i.operands != 1) { d.bad = true; break; }
        d.write(i.a, true, false);
        break;
    case IrSem::Push:
        if (i.operands != 1) { d.bad = true; break; }
        d.read(i.a, false);
        s.use |= bit(kRsp); s.part |= bit(kRsp); s.fixed |= bit(kRsp);
        s.memWrite = true;
        break;
    case IrSem::Pop:
        if (i.operands != 1) { d.bad = true; break; }
        d.write(i.a, false, true);
        s.use |= bit(kRsp); s.part |= bit(kRsp); s.fixed |= bit(kRsp);
        break;
    case IrSem::Cqo:
    case IrSem::Cdq:
        if (i.operands != 0) { d.bad = true; break; }
        s.use |= bit(kRax); s.fixed |= bit(kRax);
        s.def |= bit(kRdx);
        break;
    case IrSem::Cltq:
        if (i.operands != 0) { d.bad = true; break; }
        s.use |= bit(kRax); s.fixed |= bit(kRax);
        s.def |= bit(kRax);
        break;
    case IrSem::Div:
        if (i.operands != 1) { d.bad = true; break; }
        d.read(i.a, false);
        s.use |= bit(kRax) | bit(kRdx); s.fixed |= bit(kRax) | bit(kRdx);
        s.def |= bit(kRax) | bit(kRdx);
        break;
    case IrSem::X87:
        if (i.operands > 1) { d.bad = true; break; }
        if (i.operands == 1) {
            d.read(i.a, false);
            if (isX87Store(i.m)) s.memWrite = true;
        }
        break;
    case IrSem::Call:
        if (i.operands == 1) d.read(i.a, false);
        s.use = kAll; s.fixed = kAll;
        s.memWrite = true;
        break;
    case IrSem::Jump:
        if (i.operands == 1) d.read(i.a, false);
        s.use = kAll; s.fixed = kAll;
        break;
    case IrSem::Ret:
        s.use = kRetLive; s.fixed = kRetLive;
        break;
    case IrSem::Leave:
        s.use |= bit(kRbp) | bit(kRsp); s.fixed |= bit(kRbp) | bit(kRsp);
        s.part |= bit(kRbp) | bit(kRsp);
        break;
    case IrSem::Nop:
        break;
    case IrSem::Unknown:
        d.bad = true;
        break;
    }
    if (d.bad) d.everything();
    return s;
}

// Whether `o` names canonical register `r`, and at what width. A memory
// base or an indirect target is always the 64-bit name.
bool namesReg(const IrOp &o, int r, int &width) {
    if (o.kind != Op::Reg && o.kind != Op::Mem && o.kind != Op::Ind) return false;
    int w;
    if (regLookup(o.text, w) != r) return false;
    width = w;
    return true;
}

// **Whether operand k of x is a read position** - a place a rename of the
// register it names is a rename of a read. A memory base or an indirect
// target is one always; a register is one where its class reads that side.
bool readsAt(const IrIns &x, const IrSem &s, int k) {
    const IrOp &o = k == 0 ? x.a : x.b;
    if (o.kind == Op::Mem || o.kind == Op::Ind) return true;
    if (o.kind != Op::Reg) return false;
    switch (s.cls) {
    case IrSem::Move: case IrSem::Rmw: case IrSem::Push: case IrSem::Div:
    case IrSem::Call: case IrSem::Jump:
        return k == 0;
    case IrSem::Cmp:
        return true;
    default:
        return false;
    }
}

bool isGpr64(const IrOp &o, int &canon) {
    int w;
    if (o.kind != Op::Reg) return false;
    canon = regLookup(o.text, w);
    return canon >= 0 && canon < kGprCount && w == 8;
}

// A general register a pass may allocate: any but the two that address the frame.
bool allocatable(int r) {
    return r >= 0 && r < kGprCount && r != kRbp && r != kRsp;
}

bool isReg(const IrOp &o, const char *name) {
    return o.kind == Op::Reg && o.text == name;
}

bool isZeroImm(const IrOp &o) {
    return o.kind == Op::Imm && o.immNumeric && o.uimm == 0;
}

IrOp regOp(const char *name) {
    IrOp o;
    o.kind = Op::Reg;
    o.text = name;
    return o;
}

void refresh(IrIns &x) {
    x.sem = describe(x);
    x.semValid = true;
}

// How far forward a pass looks from one instruction: a long run costs its length.
const std::size_t kScanLimit = 256;

// A run ends at anything that can leave it other than by falling through.
bool endsRun(const std::string &m) {
    return (!m.empty() && m[0] == 'j') || m == "call" || m == "ret";
}

// **The general registers whose 64-bit self-move is a true no-op.** Moving
// a 32-bit register onto itself zero-extends into the whole register, and a
// 16- or 8-bit one is a partial write; only these names may be dropped.
bool is64Gpr(const std::string &r) {
    int w;
    int c = regLookup(r, w);
    return c >= 0 && c < kGprCount && w == 8;
}

// **The 64-bit move is spelled `mov` by the walker and `movq` in a few
// places**; with a 64-bit general register on one side they are the same
// instruction, and only then is the width certain.
bool isMov64(const IrIns &i) {
    return (i.m == "mov" || i.m == "movq") && i.operands == 2;
}

bool isRegToMem(const IrIns &i) {
    return isMov64(i) && i.a.kind == Op::Reg && i.b.kind == Op::Mem &&
           is64Gpr(i.a.text);
}

bool isMemToReg(const IrIns &i) {
    return isMov64(i) && i.a.kind == Op::Mem && i.b.kind == Op::Reg &&
           is64Gpr(i.b.text);
}

// A push or pop of a 64-bit general register other than the two that address
// the stack, which is every one the walker's expression stack uses.
bool isStackOp(const IrIns &i, const char *m) {
    return i.m == m && i.operands == 1 && i.a.kind == Op::Reg &&
           is64Gpr(i.a.text) && i.a.text != "%rsp" && i.a.text != "%rbp";
}

} // namespace

// ---------------------------------------------------------------------------
// Collecting a run.

void Optimizer::add(IrIns &&i) {
    // Nothing reaches here: the last run ended in a jump or a return and no
    // label has been defined since.
    if (unreachable_ && level_ >= 1) {
        removed_++;
        return;
    }
    bool ends = endsRun(i.m);
    bool leaves = i.m == "jmp" || i.m == "ret";
    run_.push_back(std::move(i));
    if (ends) {
        endRun();
        unreachable_ = leaves;
    }
}

void Optimizer::ins(const std::string &m) {
    IrIns i; i.m = m; i.operands = 0;
    add(std::move(i));
}

void Optimizer::ins(const std::string &m, const Op &a) {
    IrIns i; i.m = m; i.operands = 1; i.a = IrOp::from(a);
    add(std::move(i));
}

void Optimizer::ins(const std::string &m, const Op &a, const Op &b) {
    IrIns i; i.m = m; i.operands = 2; i.a = IrOp::from(a); i.b = IrOp::from(b);
    add(std::move(i));
}

void Optimizer::endRun() {
    if (run_.empty()) return;
    if (level_ >= 1) optimize();
    replay();
}

void Optimizer::replay() {
    for (const IrIns &i : run_) {
        switch (i.operands) {
        case 0: under_->ins(i.m); break;
        case 1: under_->ins(i.m, i.a.view()); break;
        default: under_->ins(i.m, i.a.view(), i.b.view()); break;
        }
    }
    run_.clear();
}

// The text is about to be read, cut, or continued by something that is not
// an instruction - which may be a label, so what follows is reachable.
void Optimizer::flush() {
    endRun();
    unreachable_ = false;
}

void Optimizer::interrupt() { flush(); }

// ---------------------------------------------------------------------------
// The passes.

void Optimizer::optimize() {
    peephole();
    for (int round = 0; round < 4; round++) {
        bool changed = false;
        analyse();
        if (dropExtensions()) { changed = true; compact(); analyse(); }
        if (fuseLeas())       { changed = true; compact(); analyse(); }
        if (pairStack())      { changed = true; compact(); analyse(); }
        if (retargetDefs())   { changed = true; compact(); analyse(); }
        if (propagateCopies()){ changed = true; compact(); }
        if (!changed) break;
    }
    analyse();
    shorten();
    compact();
}

void Optimizer::compact() {
    kept_.clear();
    kept_.reserve(run_.size());
    for (IrIns &i : run_)
        if (!i.dead) kept_.push_back(std::move(i));
    run_.swap(kept_);
}

// **What each instruction does, and what is live after it.** Backward over
// the run; the end of a run is a label or a branch, where everything is
// live - except after a `ret`, where only what a caller may read is.

// Flags likewise, and dead at a `call` or a `ret`: no ABI passes them.
void Optimizer::analyse() {
    const std::size_t n = run_.size();
    liveOut_.resize(n);
    flagsLiveOut_.resize(n);
    for (std::size_t i = 0; i < n; i++)
        if (!run_[i].semValid) refresh(run_[i]);

    unsigned live = kAll;
    bool flags = true;
    if (n != 0) {
        const IrSem::Class last = run_[n - 1].sem.cls;
        if (last == IrSem::Ret) live = 0;
        if (last == IrSem::Ret || last == IrSem::Call) flags = false;
    }
    for (std::size_t k = n; k-- > 0;) {
        const IrSem &s = run_[k].sem;
        liveOut_[k] = live;
        flagsLiveOut_[k] = flags ? 1 : 0;
        live = s.use | (live & ~s.def);
        flags = s.flagsUse || (flags && !s.flagsDef);
    }
}

// **Each pattern is between an instruction and the last one kept**, which is
// what lets a chain fall: a store and two reloads lose both reloads. The first
// three never fire on what the walker writes today; the fourth is its idiom.
void Optimizer::peephole() {
    std::vector<IrIns> &kept = kept_;
    kept.clear();
    kept.reserve(run_.size());
    for (IrIns &i : run_) {
        // mov %r, %r: a 64-bit self-move does nothing.
        if (isMov64(i) && i.a.kind == Op::Reg && i.b.kind == Op::Reg &&
            i.a.text == i.b.text && is64Gpr(i.a.text)) {
            removed_++;
            continue;
        }
        if (!kept.empty() && isMemToReg(i)) {
            const IrIns &p = kept.back();
            // mov %r, m ; mov m, %r: the reload reads back what %r still holds.
            if (isRegToMem(p) && p.a.text == i.b.text && p.b.same(i.a)) {
                removed_++;
                continue;
            }
            // mov m, %r ; mov m, %r: the second load reads what the first did,
            // unless %r is the address it reads through.
            if (isMemToReg(p) && p.a.same(i.a) && p.b.text == i.b.text &&
                i.a.text.find(i.b.text) == std::string::npos) {
                removed_++;
                continue;
            }
        }
        // push %x ; pop %y is mov %x, %y, and nothing at all when %y is %x: rsp
        // and the flags end as they began, and the word written below rsp is
        // one nothing reads - the frame is addressed from rbp, the stack upward.

        // **At both levels, though it looks like a size-for-speed trade.** The
        // pair is two bytes and the mov three, so it was gated to -O2 once and
        // measured: .text came out 51,408 bytes LARGER at -O1, not smaller.

        // It is an enabling transformation - once the pair is a mov, copy
        // propagation sees through it and usually deletes it. Withholding it
        // costs both size and speed, so it is not a knob.
        if (!kept.empty() && isStackOp(i, "pop") && isStackOp(kept.back(), "push")) {
            IrIns &p = kept.back();
            if (p.a.text == i.a.text) {
                kept.pop_back();
                removed_ += 2;
            } else {
                p.m = "mov";
                p.operands = 2;
                p.b = i.a;
                p.semValid = false;
                removed_++;
            }
            continue;
        }
        kept.push_back(std::move(i));
    }
    run_.swap(kept);
}

// **A sign- or zero-extension of a register that already is one.** The
// walker widens every 32-bit result with `movslq %eax, %rax` and does not
// remember having just done so; the state here does.

// Forward over the run, with what is known of each register cleared by any
// write to it.
bool Optimizer::dropExtensions() {
    enum { kSext32 = 1, kZext8 = 2, kZext16 = 4 };
    unsigned char known[kGprCount] = { 0 };
    bool changed = false;
    for (std::size_t i = 0; i < run_.size(); i++) {
        IrIns &x = run_[i];
        const IrSem &s = run_[i].sem;
        if (s.cls == IrSem::Move && x.a.kind == Op::Reg && x.b.kind == Op::Reg) {
            int wa, wb;
            int ra = regLookup(x.a.text, wa);
            int rb = regLookup(x.b.text, wb);
            if (ra >= 0 && ra == rb && rb < kGprCount && (wb == 8 || wb == 4)) {
                bool noop = false;
                if (x.m == "movslq" && wa == 4 && wb == 8) noop = (known[rb] & kSext32) != 0;
                else if ((x.m == "movzbq" || x.m == "movzbl") && wa == 1)
                    noop = (known[rb] & kZext8) != 0;
                else if ((x.m == "movzwq" || x.m == "movzwl") && wa == 2)
                    noop = (known[rb] & kZext16) != 0;
                if (noop) {
                    x.dead = true;
                    removed_++;
                    changed = true;
                    continue;
                }
            }
        }
        if (s.cls == IrSem::Unknown) {
            for (int r = 0; r < kGprCount; r++) known[r] = 0;
            continue;
        }
        const unsigned written = s.def | s.part;
        for (int r = 0; r < kGprCount; r++)
            if (written & bit(r)) known[r] = 0;
        if (s.cls == IrSem::Cltq) {
            known[kRax] = kSext32;
        } else if (s.cls == IrSem::Move && s.dstReg >= 0 && s.dstReg < kGprCount) {
            unsigned char k = 0;
            const std::string &m = x.m;
            if (m == "movslq" || m == "movsbq" || m == "movswq") {
                k = kSext32;
            } else if (m == "movzbq" || m == "movzbl") {
                k = kSext32 | kZext8 | kZext16;
            } else if (m == "movzwq" || m == "movzwl") {
                k = kSext32 | kZext16;
            } else if ((m == "mov" || m == "movq" || m == "movl" || m == "movabs") &&
                       x.a.kind == Op::Imm && x.a.immNumeric) {
                if (!x.a.immNeg) {
                    if (x.a.uimm < 128) k = kSext32 | kZext8 | kZext16;
                    else if (x.a.uimm < 65536) k = kSext32 | kZext16;
                    else if (x.a.uimm < (1ull << 31)) k = kSext32;
                } else if (s.dstWidth == 8 && x.a.uimm <= (1ull << 31)) {
                    k = kSext32;
                }
            } else if ((m == "mov" || m == "movq") && x.a.kind == Op::Reg && s.dstWidth == 8) {
                int wa;
                int ra = regLookup(x.a.text, wa);
                if (ra >= 0 && ra < kGprCount && wa == 8) k = known[ra];
            }
            known[s.dstReg] = k;
        }
    }
    return changed;
}

// **`lea M, %r` followed by a use of memory at (%r)**: the address goes into
// the operand and the lea goes, when %r is read nowhere else in that
// instruction and its value is dead afterwards.

// Dead because that instruction overwrites it, or because nothing later
// reads it before writing it.
bool Optimizer::fuseLeas() {
    bool changed = false;
    for (std::size_t i = 0; i + 1 < run_.size(); i++) {
        IrIns &l = run_[i];
        if (l.dead || l.m != "lea" || l.operands != 2) continue;
        if (l.a.kind != Op::Mem && l.a.kind != Op::Rip) continue;
        int r;
        if (!isGpr64(l.b, r) || !allocatable(r)) continue;

        IrIns &x = run_[i + 1];
        if (x.dead) continue;
        const IrSem &s = run_[i + 1].sem;
        if (s.cls == IrSem::Unknown) continue;

        IrOp *mo = nullptr;
        if (x.operands >= 1 && x.a.kind == Op::Mem && x.a.text == l.b.text) mo = &x.a;
        else if (x.operands == 2 && x.b.kind == Op::Mem && x.b.text == l.b.text) mo = &x.b;
        if (mo == nullptr) continue;

        IrOp fused = l.a;
        if (l.a.kind == Op::Rip) {
            // The spelling of a rip-relative operand carries no displacement.
            if (mo->hasDisp && mo->disp != 0) continue;
        } else {
            // A push or pop moves rsp before or after the access; keep clear.
            if (l.a.text == "%rsp" && (s.cls == IrSem::Push || s.cls == IrSem::Pop)) continue;
            long long d = (l.a.hasDisp ? l.a.disp : 0) + (mo->hasDisp ? mo->disp : 0);
            if (d > 0x7fffffffLL || d < -0x80000000LL) continue;
            fused.disp = d;
            fused.hasDisp = d != 0;
        }

        // The instruction as it would be, so its reads are the real ones.
        const IrOp saved = *mo;
        *mo = fused;
        const IrSem t = describe(x);
        const bool dead = (t.def & bit(r)) != 0 || (liveOut_[i + 1] & bit(r)) == 0;
        if (t.cls == IrSem::Unknown || (t.use & bit(r)) != 0 || !dead) {
            *mo = saved;
            continue;
        }
        x.sem = t;
        x.semValid = true;
        l.dead = true;
        removed_++;
        changed = true;
    }
    return changed;
}

// **A push and the pop that takes it back**, with nothing between that moves
// rsp or reads the stack: the same register both times, and both go; another
// register untouched between, and the push is a move to it.

// Otherwise the pushed register intact until the pop, and the pop is a move
// from it. The word below rsp is one nothing reads, as peephole() says.
bool Optimizer::pairStack() {
    bool changed = false;
    const std::size_t n = run_.size();
    for (std::size_t i = 0; i < n; i++) {
        IrIns &p = run_[i];
        if (p.dead || !isStackOp(p, "push")) continue;
        int x;
        if (!isGpr64(p.a, x)) continue;
        bool xChanged = false;
        unsigned touched = 0;
        for (std::size_t j = i + 1; j < n && j - i <= kScanLimit; j++) {
            IrIns &q = run_[j];
            if (q.dead) continue;
            const IrSem &s = run_[j].sem;
            if (s.cls == IrSem::Unknown) break;
            if (isStackOp(q, "pop")) {
                int y;
                if (!isGpr64(q.a, y)) break;
                if (y == x) {
                    if (xChanged) break;
                    p.dead = q.dead = true;
                    removed_ += 2;
                } else if ((touched & bit(y)) == 0) {
                    p.m = "mov"; p.operands = 2; p.b = q.a;
                    q.dead = true;
                    removed_++;
                    refresh(p);
                } else if (!xChanged) {
                    q.m = "mov"; q.operands = 2; q.b = q.a; q.a = p.a;
                    p.dead = true;
                    removed_++;
                    refresh(q);
                } else {
                    break;
                }
                changed = true;
                break;
            }
            if ((s.use | s.def | s.part) & bit(kRsp)) break;
            touched |= s.use | s.def | s.part;
            if ((s.def | s.part) & bit(x)) xChanged = true;
        }
    }
    return changed;
}

// **A register written whole and at once copied elsewhere**, its first home
// dead after the copy: write it to the second home in the first place.
//   movq -8(%rbp), %rax ; mov %rax, %rdi   ->   movq -8(%rbp), %rdi
bool Optimizer::retargetDefs() {
    bool changed = false;
    for (std::size_t i = 0; i + 1 < run_.size(); i++) {
        IrIns &x = run_[i];
        const IrSem &s = run_[i].sem;
        if (x.dead || (s.cls != IrSem::Move && s.cls != IrSem::Pop)) continue;
        if (!allocatable(s.dstReg)) continue;

        IrIns &c = run_[i + 1];
        if (c.dead || !isMov64(c)) continue;
        int ra, rb;
        if (!isGpr64(c.a, ra) || !isGpr64(c.b, rb)) continue;
        if (ra != s.dstReg || !allocatable(rb) || rb == ra) continue;
        if (liveOut_[i + 1] & bit(ra)) continue;

        IrOp &dst = s.cls == IrSem::Pop ? x.a : x.b;
        dst.text = regName(rb, s.dstWidth);
        refresh(x);
        c.dead = true;
        removed_++;
        changed = true;
    }
    return changed;
}

// **A copy whose destination dies before the run ends, or is overwritten in
// it**: every read of the destination between reads the source instead,
// and the copy goes.

// The source must be intact at each of those reads, and each must be an
// operand a rename can reach - not an implicit one, not the shift count,
// not the destination of a read-modify-write.
bool Optimizer::propagateCopies() {
    bool changed = false;
    const std::size_t n = run_.size();
    std::vector<std::size_t> uses;
    for (std::size_t i = 0; i < n; i++) {
        IrIns &c = run_[i];
        if (c.dead || !isMov64(c)) continue;
        int ra, rb;
        if (!isGpr64(c.a, ra) || !isGpr64(c.b, rb)) continue;
        if (!allocatable(ra) || !allocatable(rb) || ra == rb) continue;

        uses.clear();
        bool ok = true, dead = false, sourceChanged = false;
        int penalty = 0;
        for (std::size_t j = i + 1; j < n && ok; j++) {
            if (j - i > kScanLimit) { ok = false; break; }
            const IrIns &x = run_[j];
            if (x.dead) continue;
            const IrSem &s = run_[j].sem;
            if (s.cls == IrSem::Unknown) { ok = false; break; }
            if (s.use & bit(rb)) {
                if (sourceChanged || (s.fixed & bit(rb))) { ok = false; break; }
                int w;
                for (int k = 0; k < x.operands; k++) {
                    const IrOp &o = k == 0 ? x.a : x.b;
                    if (!namesReg(o, rb, w) || !readsAt(x, s, k)) continue;
                    if (w == 0 || w == 16) { ok = false; break; }
                    if (o.kind != Op::Reg && w != 8) { ok = false; break; }
                    if (w != 8 && needsRex(ra, w) && !needsRex(rb, w)) penalty++;
                }
                if (!ok) break;
                uses.push_back(j);
            }
            if (s.def & bit(rb)) { dead = true; break; }
            if (s.part & bit(rb)) { ok = false; break; }
            if ((s.def | s.part) & bit(ra)) sourceChanged = true;
        }
        if (!ok) continue;
        if (!dead) dead = (liveOut_[n - 1] & bit(rb)) == 0;
        if (!dead) continue;
        // Each rewritten name may grow by a REX byte; the copy is three.
        if (penalty >= 3) continue;

        for (std::size_t j : uses) {
            IrIns &x = run_[j];
            int w;
            for (int k = 0; k < x.operands; k++) {
                IrOp &o = k == 0 ? x.a : x.b;
                if (namesReg(o, rb, w) && readsAt(x, run_[j].sem, k)) o.text = regName(ra, w);
            }
            refresh(x);
        }
        c.dead = true;
        removed_++;
        changed = true;
    }
    return changed;
}

// **The same instruction in fewer bytes.** None of these changes what a
// register holds; the one that touches the flags is taken only where the
// flags are dead.
void Optimizer::shorten() {
    const std::size_t n = run_.size();
    for (std::size_t i = 0; i < n; i++) {
        IrIns &x = run_[i];
        if (x.dead) continue;
        x.semValid = false;

        // mov $imm, %r64 with imm in [0, 2^32): movl $imm, %r32 zero-extends
        // and is two bytes shorter, five where the assembler chose movabs;
        // and zero, where nothing reads the flags, is xor %r32, %r32.
        if ((x.m == "mov" || x.m == "movq" || x.m == "movabs") && x.operands == 2 &&
            x.a.kind == Op::Imm && x.a.immNumeric) {
            int r;
            if (isGpr64(x.b, r)) {
                if (!x.a.immNeg && x.a.uimm < (1ull << 32)) {
                    if (x.a.uimm == 0 && !flagsLiveOut_[i]) {
                        x.m = "xor";
                        x.a = regOp(regName(r, 4));
                        x.b = x.a;
                    } else {
                        x.m = "movl";
                        x.b.text = regName(r, 4);
                    }
                } else if (x.m == "movabs" && x.a.immNeg && x.a.uimm <= (1ull << 31)) {
                    x.m = "mov";
                }
                continue;
            }
        }
        if (x.m == "movl" && x.operands == 2 && isZeroImm(x.a) && x.b.kind == Op::Reg &&
            !flagsLiveOut_[i]) {
            int w;
            int r = regLookup(x.b.text, w);
            if (r >= 0 && r < kGprCount && w == 4) {
                x.m = "xor";
                x.a = x.b;
            }
            continue;
        }

        // cmp $0, %r sets every flag as test %r, %r does, one byte shorter.
        if ((x.m == "cmp" || x.m == "cmpq" || x.m == "cmpl" || x.m == "cmpw" || x.m == "cmpb") &&
            x.operands == 2 && isZeroImm(x.a) && x.b.kind == Op::Reg) {
            int w;
            int r = regLookup(x.b.text, w);
            if (r >= 0 && r < kGprCount && w != 0) {
                x.m = "test" + x.m.substr(3);
                x.a = x.b;
            }
            continue;
        }

        // mov %rbp, %rsp ; pop %rbp is leave, one byte for four.
        if (x.m == "mov" && x.operands == 2 && isReg(x.a, "%rbp") && isReg(x.b, "%rsp") &&
            i + 1 < n && run_[i + 1].m == "pop" && run_[i + 1].operands == 1 &&
            isReg(run_[i + 1].a, "%rbp")) {
            x.m = "leave";
            x.operands = 0;
            run_[i + 1].dead = true;
            removed_++;
            continue;
        }
    }
}

// ---------------------------------------------------------------------------
// Everything else ends the run and goes through in order.

void Optimizer::defLabel(const std::string &l) { interrupt(); under_->defLabel(l); }
void Optimizer::functionBegin(const std::string &name, bool exported, bool mergeable) {
    interrupt(); under_->functionBegin(name, exported, mergeable);
}
void Optimizer::prologue(int frameSize, const std::string &lsda) {
    interrupt(); under_->prologue(frameSize, lsda);
}
void Optimizer::functionEnd(const std::string &name) { interrupt(); under_->functionEnd(name); }
void Optimizer::fileEntry(int n, const std::string &name) { interrupt(); under_->fileEntry(n, name); }
void Optimizer::location(int file, int line, int column) {
    interrupt(); under_->location(file, line, column);
}
void Optimizer::predefine(const std::vector<std::string> &names) { interrupt(); under_->predefine(names); }
void Optimizer::preamble(std::ostream &o) { interrupt(); under_->preamble(o); }
void Optimizer::postamble(std::ostream &o) { interrupt(); under_->postamble(o); }
void Optimizer::globl(const std::string &name) { interrupt(); under_->globl(name); }
void Optimizer::weakDefinition(const std::string &name) { interrupt(); under_->weakDefinition(name); }
void Optimizer::textSection() { interrupt(); under_->textSection(); }
void Optimizer::rodataSection() { interrupt(); under_->rodataSection(); }
void Optimizer::dataSection() { interrupt(); under_->dataSection(); }
void Optimizer::bssSection() { interrupt(); under_->bssSection(); }
void Optimizer::objectType(const std::string &name) { interrupt(); under_->objectType(name); }
void Optimizer::objectSize(const std::string &name, int size) { interrupt(); under_->objectSize(name, size); }
void Optimizer::align(int n) { interrupt(); under_->align(n); }
void Optimizer::zero(int n) { interrupt(); under_->zero(n); }
void Optimizer::dataInt(int size, long long v) { interrupt(); under_->dataInt(size, v); }
void Optimizer::dataSym(const std::string &sym, long long off) { interrupt(); under_->dataSym(sym, off); }
void Optimizer::noteHasEh(bool yes) { interrupt(); under_->noteHasEh(yes); }
void Optimizer::initialiserEntry(const std::string &fn, bool dsoHandle) {
    interrupt(); under_->initialiserEntry(fn, dsoHandle);
}
std::string Optimizer::labelText(const std::string &l) const { return under_->labelText(l); }
void Optimizer::dataBytes(const std::string &bytes) { interrupt(); under_->dataBytes(bytes); }
