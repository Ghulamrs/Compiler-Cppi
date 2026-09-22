#include "Spelling.h"

#include <ostream>
#include <string>

void GnuSpelling::op(const Op &x) {
    switch (x.kind) {
    case Op::Reg: o_ += x.text; return;
    case Op::Imm:
        o_ += '$';
        if (!x.immNumeric) { o_ += x.text; return; }
        if (x.immNeg) o_ += '-';
        appendNum(o_, x.uimm);
        return;
    case Op::Mem:
        if (x.hasDisp) appendNum(o_, x.disp);
        o_ += '(';
        o_ += x.text;
        o_ += ')';
        return;
    case Op::Rip: o_ += sym(std::string(x.text.p, x.text.n)); o_ += "(%rip)"; return;
    case Op::Ind: o_ += '*'; o_ += x.text; return;
    case Op::Lbl: o_ += sym(std::string(x.text.p, x.text.n)); return;
    }
}

void GnuSpelling::ins(const std::string &m) {
    lastWasCall_ = m == "call";
    o_ += "  "; o_ += m; o_ += '\n';
}

void GnuSpelling::ins(const std::string &m, const Op &a) {
    lastWasCall_ = m == "call";
    o_ += "  "; o_ += m; o_ += ' ';
    op(a);
    o_ += '\n';
}

void GnuSpelling::ins(const std::string &m, const Op &a, const Op &b) {
    lastWasCall_ = m == "call";
    o_ += "  "; o_ += m; o_ += ' ';
    op(a);
    o_ += ", ";
    op(b);
    o_ += '\n';
}

void GnuSpelling::defLabel(const std::string &l) { o_ += sym(l); o_ += ":\n"; }
void GnuSpelling::loopAlign() { o_ += "  .p2align 4\n"; }

// The flag is for COFF, where a mergeable definition needs its section opened
// before the label. ELF and Mach-O say it afterwards, with `.weak`, exactly as
// they did - so this ignores it and the emitted text is unchanged.
void GnuSpelling::functionBegin(const std::string &name, bool exported,
                                bool mergeable) {
    (void)mergeable;
    if (exported) globl(name);
    textSection();
    // **A function starts on a 16-byte boundary**, as gcc and clang place
    // them: the Itanium member pointer keeps "virtual" in a code word's low
    // bit, and a function at an odd address read as a vtable offset.
    o_ += "  .p2align 4\n";
    defLabel(name);
}

// **Unwind data, and it is the same three directives in every function.** A
// cxx1 frame has one shape, so the CFA is rbp + 16 throughout; without it a
// backtrace stops here and no exception passes. MASM has always said this.

// The callee-saved registers in the prologue's order, as the Optimizer indexes
// them, and as the unwinder numbers them (rbx is 3).
static const char *const kSavedNames[] = { "%rbx", "%r12", "%r13", "%r14", "%r15" };
static const int kSavedIndex[] = { 1, 12, 13, 14, 15 };
static const int kUnwindReg[] = { 3, 12, 13, 14, 15 };

void GnuSpelling::prologue(int frameSize, const std::string &lsda, unsigned saved,
                           int outgoing) {
    saved_ = saved;
    savedFrame_ = frameSize;
    int n = 0;
    for (int k = 0; k < 5; k++) if (saved & (1u << kSavedIndex[k])) n++;
    o_ += "  .cfi_startproc\n";
    if (!lsda.empty()) {
        o_ += "  .cfi_personality 155, DW.ref.__gxx_personality_v0\n";
        o_ += "  .cfi_lsda 27, " + lsda + "\n";
    }
    ins("push", reg("%rbp"));
    o_ += "  .cfi_def_cfa_offset 16\n";
    o_ += "  .cfi_offset %rbp, -16\n";
    ins("mov", reg("%rsp"), reg("%rbp"));
    o_ += "  .cfi_def_cfa_register %rbp\n";
    // **The outgoing area goes under everything**, so with nothing to save it
    // and the frame are one allocation.
    if (n == 0) {
        if (frameSize + outgoing > 0) ins("sub", imm(frameSize + outgoing), reg("%rsp"));
        return;
    }
    if (frameSize > 0) ins("sub", imm(frameSize), reg("%rsp"));
    // **The saves go below the frame**, pushed after it so every offset the
    // walker wrote against rbp stands; an odd count takes eight more to keep
    // the stack at sixteen. Each is told to the unwinder against the CFA.
    n = 0;
    for (int k = 0; k < 5; k++) {
        if (!(saved & (1u << kSavedIndex[k]))) continue;
        ins("push", reg(kSavedNames[k]));
        n++;
        o_ += "  .cfi_offset " + std::string(kSavedNames[k]) + ", -";
        appendNum(o_, static_cast<unsigned long long>(16 + frameSize + 8 * n));
        o_ += '\n';
    }
    const int below = (n % 2 == 1 ? 8 : 0) + outgoing;
    if (below > 0) ins("sub", imm(below), reg("%rsp"));
}

void GnuSpelling::restoreSaved() {
    int n = 0;
    for (int k = 0; k < 5; k++) {
        if (!(saved_ & (1u << kSavedIndex[k]))) continue;
        n++;
        ins("mov", mem(-(savedFrame_ + 8 * n), "%rbp"), reg(kSavedNames[k]));
    }
}

void GnuSpelling::functionEnd(const std::string &) {
    o_ += "  .cfi_endproc\n";
}

void GnuSpelling::globl(const std::string &name) {
    o_ += "  .globl "; o_ += sym(name); o_ += '\n';
}

// Measured from clang: `.weak` beside the `.globl`, which is what makes the
// linker fold the copies of an inline function rather than reject them.
void GnuSpelling::weakDefinition(const std::string &name) {
    o_ += "  .weak "; o_ += sym(name); o_ += '\n';
}

void GnuSpelling::fileEntry(int n, const std::string &name) {
    o_ += "  .file ";
    appendNum(o_, n);
    o_ += " \"";
    o_ += name;
    o_ += "\"\n";
}

void GnuSpelling::location(int file, int line, int column) {
    o_ += "  .loc ";
    appendNum(o_, file);
    o_ += ' ';
    appendNum(o_, line);
    o_ += ' ';
    appendNum(o_, column);
    o_ += '\n';
}

// Measured from clang: the function's address in .init_array, and
// __dso_handle declared hidden where the file hands it to __cxa_atexit.
void GnuSpelling::initialiserEntry(const std::string &fn, bool dsoHandle) {
    if (dsoHandle) o_ += "  .hidden __dso_handle\n";
    o_ += "  .section .init_array,\"aw\",@init_array\n  .p2align 3, 0x0\n  .quad ";
    o_ += sym(fn);
    o_ += '\n';
}

void GnuSpelling::textSection()   { o_ += "  .text\n"; }
void GnuSpelling::rodataSection() { o_ += "  .section .rodata\n"; }
void GnuSpelling::dataSection()   { o_ += "  .data\n"; }
void GnuSpelling::bssSection()    { o_ += "  .bss\n"; }

void GnuSpelling::objectType(const std::string &name) {
    o_ += "  .type "; o_ += sym(name); o_ += ", @object\n";
}

void GnuSpelling::objectSize(const std::string &name, int size) {
    o_ += "  .size "; o_ += sym(name); o_ += ", "; appendNum(o_, size); o_ += '\n';
}

void GnuSpelling::align(int n) { o_ += "  .align "; appendNum(o_, n); o_ += '\n'; }
void GnuSpelling::zero(int n)  { o_ += "  .zero ";  appendNum(o_, n); o_ += '\n'; }

void GnuSpelling::dataInt(int size, long long v) {
    switch (size) {
    case 1: o_ += "  .byte "; break;
    case 2: o_ += "  .word "; break;
    case 4: o_ += "  .long "; break;
    default: o_ += "  .quad "; break;
    }
    appendNum(o_, v);
    o_ += '\n';
}

void GnuSpelling::dataSym(const std::string &s, long long off) {
    o_ += "  .quad ";
    o_ += sym(s);
    if (off > 0) { o_ += '+'; appendNum(o_, off); }
    else if (off < 0) { o_ += '-'; appendNum(o_, -off); }
    o_ += '\n';
}

void GnuSpelling::dataBytes(const std::string &bytes) {
    o_ += "  .byte ";
    for (std::size_t k = 0; k < bytes.size(); k++) {
        if (k) o_ += ", ";
        appendNum(o_, static_cast<long long>(
                          static_cast<unsigned char>(bytes[k])));
    }
    o_ += '\n';
}


// --- CoffSpelling -----------------------------------------------------------

// Quoted where GNU-as would not take the name as an identifier. A Microsoft
// mangled name always carries a '?' or an '@'; a C name carries neither, so
// `printf` and `__CxxFrameHandler3` are written plainly, as clang writes them.
std::string CoffSpelling::sym(const std::string &name) const {
    std::string n = name;
    // **A `.L` label is a temporary, and a table cannot name one.**
    if (n.compare(0, 2, ".L") == 0) {
        std::string out = "$";
        for (char c : n) out += (c == '.') ? '_' : c;
        n = out;
    }
    if (n.find('?') == std::string::npos && n.find('@') == std::string::npos &&
        n.find('$') == std::string::npos)
        return n;
    return "\"" + n + "\"";
}

void CoffSpelling::functionBegin(const std::string &name, bool exported,
                                 bool mergeable) {
    fnName_ = name;
    mergeable_ = mergeable;
    if (mergeable) {
        // The section carries the COMDAT bit and names the symbol it folds on;
        // `discard` is IMAGE_COMDAT_SELECT_ANY, which is what an inline
        // definition wants - keep one, drop the rest.
        o_ += "  .section .text,\"xr\",discard," + sym(name) + "\n";
        opened_ = name;
    } else {
        textSection();
    }
    if (exported) globl(name);
    defLabel(name);
}

// For a global the code generator says this *before* the label, which is where
// the section directive has to go; for a function functionBegin has already
// opened one, and a second would start an empty section.
void CoffSpelling::weakDefinition(const std::string &name) {
    if (opened_ == name) { opened_.clear(); return; }
    // **The COMDAT takes the plain section's own attributes**: a template's
    // static member and its guard are writable, and clang puts them in
    // `.bss,"bw",discard` - measured. Read-only only where the section is.
    const char *where = std::strstr(plainSection_, ".bss") != nullptr
                          ? ".bss,\"bw\""
                      : std::strstr(plainSection_, ".data") != nullptr
                          ? ".data,\"dw\""
                          : ".rdata,\"dr\"";
    o_ += "  .section " + std::string(where) + ",discard," + sym(name) + "\n";
    comdatData_ = 1;
}

// Measured from clang for x86_64-pc-windows-msvc: the same pointer, in the
// section the CRT walks before main. **`.CRT$XCU` carries no COMDAT clause**,
// and the `unique,0` that used to be here is why this is a comment.
void CoffSpelling::initialiserEntry(const std::string &fn, bool) {
    o_ += "  .section .CRT$XCU,\"dr\"\n  .p2align 3, 0x0\n  .quad ";
    o_ += sym(fn);
    o_ += '\n';
}

// COFF spells the read-only segment .rdata, and has no .type or .size. Each
// plain section is remembered, so a data COMDAT can be closed by putting the
// plain one back - see comdatData_.
void CoffSpelling::rodataSection() {
    plainSection_ = "  .section .rdata,\"dr\"\n"; comdatData_ = 0; o_ += plainSection_;
}
void CoffSpelling::dataSection() {
    plainSection_ = "  .data\n"; comdatData_ = 0; o_ += plainSection_;
}
void CoffSpelling::bssSection() {
    plainSection_ = "  .bss\n"; comdatData_ = 0; o_ += plainSection_;
}
void CoffSpelling::align(int n) {
    // The COMDAT object's own align passes through; the next object's puts
    // the plain section back first, so it does not land in a section that is
    // somebody else's to discard.
    if (comdatData_ == 1) comdatData_ = 2;
    else if (comdatData_ == 2) { o_ += plainSection_; comdatData_ = 0; }
    GnuSpelling::align(n);
}
void CoffSpelling::restoreSaved() {
    int n = 0;
    for (int k = 0; k < 5; k++) {
        if (!(saved_ & (1u << kSavedIndex[k]))) continue;
        o_ += "  mov ";
        appendNum(o_, static_cast<unsigned long long>(outgoing_ + 8 * n));
        o_ += "(%rbp), " + std::string(kSavedNames[k]) + "\n";
        n++;
    }
}
void CoffSpelling::objectType(const std::string &name) { (void)name; }
void CoffSpelling::objectSize(const std::string &name, int size) {
    (void)name; (void)size;
}

// **Hand-written, because `.seh_handlerdata` cannot live in a COMDAT.** The
// `.seh_*` directives are the tidy way and the assembler builds .pdata and
// .xdata from them - measured, and it works for every function in plain .text.
void CoffSpelling::prologue(int frameSize, const std::string &lsda, unsigned saved,
                            int outgoing) {
    hasEh_ = !lsda.empty();
    saved_ = saved;
    outgoing_ = outgoing;
    // **The saves sit on the outgoing area at the bottom of the frame, and the
    // frame grows to hold both**: rbp is taken after the allocation, so the
    // renderer's constant grows alike and every rbp operand lands where it did.
    int n = 0;
    for (int k = 0; k < 5; k++) if (saved & (1u << kSavedIndex[k])) n++;
    frameSize += outgoing + 8 * n + (n % 2 == 1 ? 8 : 0);
    frameSize_ = frameSize;
    savedFrame_ = frameSize;
    const std::string b = "\"$LNbeg$" + fnName_ + "\"";
    o_ += b + ":\n";

    // The frame pointer is taken after the allocation on this target, so every
    // FH3 displacement is an unsigned offset up from the establisher.
    o_ += "  push %rbp\n";
    o_ += "\"$LNpush$" + fnName_ + "\":\n";
    // A frame of a page or more is probed through __chkstk first, as the
    // MASM prologue does and for the same measured reason.
    if (frameSize >= 4096) {
        o_ += "  mov $"; appendNum(o_, frameSize);
        o_ += ", %eax\n  call __chkstk\n  sub %rax, %rsp\n";
    } else if (frameSize > 0) {
        o_ += "  sub $"; appendNum(o_, frameSize); o_ += ", %rsp\n";
    }
    o_ += "\"$LNalloc$" + fnName_ + "\":\n";
    // The saves, before the frame pointer is taken: their unwind codes are
    // offsets from rsp, which is what the unwinder holds until SET_FPREG.
    std::string saves;
    n = 0;
    for (int k = 0; k < 5; k++) {
        if (!(saved & (1u << kSavedIndex[k]))) continue;
        o_ += "  mov " + std::string(kSavedNames[k]) + ", ";
        appendNum(o_, static_cast<unsigned long long>(outgoing + 8 * n));
        o_ += "(%rsp)\n\"$LNsave" + std::to_string(n) + "$" + fnName_ + "\":\n";
        // UWOP_SAVE_NONVOL is 4 with the register in the high nibble, then
        // the slot's offset from rsp in eights.
        saves = "  .byte \"$LNsave" + std::to_string(n) + "$" + fnName_ + "\"-" + b +
                "\n  .byte " + std::to_string((kUnwindReg[k] << 4) | 4) +
                "\n  .short " + std::to_string(outgoing / 8 + n) + "\n" + saves;
        n++;
    }
    o_ += "  mov %rsp, %rbp\n";
    o_ += "\"$LNprolog$" + fnName_ + "\":\n";

    // Last instruction first, which is the order an unwinder undoes them in.
    unwindCodes_ = 0;
    unwindData_.clear();
    const std::string p = "\"$LNprolog$" + fnName_ + "\"";
    const std::string al = "\"$LNalloc$" + fnName_ + "\"";
    const std::string pu = "\"$LNpush$" + fnName_ + "\"";
    // UWOP_SET_FPREG is 3; the frame offset is in the header and is zero
    // because rbp is set to rsp exactly.
    unwindData_ += "  .byte " + p + "-" + b + "\n  .byte 3\n";
    unwindCodes_ += 1;
    unwindData_ += saves;
    unwindCodes_ += 2 * n;
    if (frameSize > 0) {
        unwindData_ += "  .byte " + al + "-" + b + "\n";
        if (frameSize <= 128 && frameSize % 8 == 0) {
            // UWOP_ALLOC_SMALL is 2 with (size/8 - 1) in the high nibble.
            unwindData_ += "  .byte " +
                std::to_string(((frameSize / 8 - 1) << 4) | 2) + "\n";
            unwindCodes_ += 1;
        } else {
            unwindData_ += "  .byte 1\n  .short " +
                std::to_string((frameSize + 7) / 8) + "\n";
            unwindCodes_ += 2;
        }
    }
    // UWOP_PUSH_NONVOL is 0 with the register in the high nibble - rbp is 5.
    unwindData_ += "  .byte " + pu + "-" + b + "\n  .byte 0x50\n";
    unwindCodes_ += 1;
}

void CoffSpelling::functionEnd(const std::string &name) {
    const std::string b = "\"$LNbeg$" + name + "\"";
    const std::string e = "\"$LNend$" + name + "\"";
    const std::string u = "\"$unwind$" + name + "\"";
    o_ += e + ":\n";

    // **Associative where the function is mergeable**, so the unwind data is
    // discarded with the copy it belongs to rather than surviving it.
    const std::string assoc =
        mergeable_ ? ",associative," + sym(name) : std::string();
    o_ += "  .section .xdata,\"dr\"" + assoc + "\n";
    o_ += "  .p2align 3\n";
    o_ += u + ":\n";
    // Version 1, and the flags in the top five bits. 0x19 is EHANDLER and
    // UHANDLER together, which is what a frame with a FuncInfo needs.
    o_ += std::string("  .byte ") + (hasEh_ ? "0x19" : "0x01") + "\n";
    o_ += "  .byte \"$LNprolog$" + name + "\"-" + b + "\n";
    o_ += "  .byte " + std::to_string(unwindCodes_) + "\n";
    o_ += "  .byte 0x05\n";
    o_ += unwindData_;
    // The codes are padded to an even count, and the handler goes after.
    if (unwindCodes_ % 2 != 0) o_ += "  .short 0\n";
    if (hasEh_) {
        o_ += "  .long __CxxFrameHandler3@IMGREL\n";
        o_ += "  .long \"$cppxdata$" + name + "\"@IMGREL\n";
    }
    o_ += "  .section .pdata,\"dr\"" + assoc + "\n";
    o_ += "  .p2align 2\n";
    o_ += "  .long " + b + "@IMGREL\n";
    o_ += "  .long " + e + "@IMGREL\n";
    o_ += "  .long " + u + "@IMGREL\n";
    o_ += "  .text\n";

    unwindData_.clear();
    unwindCodes_ = 0;
    hasEh_ = false;
    mergeable_ = false;
}

// **The whole frame moved, so every offset into it moves with it.**
void CoffSpelling::op(const Op &x) {
    if (x.kind != Op::Mem || std::string(x.text.p, x.text.n) != "%rbp") {
        GnuSpelling::op(x);
        return;
    }
    long long d = (x.hasDisp ? x.disp : 0) + frameSize_;
    if (d != 0) appendNum(o_, d);
    o_ += '(';
    o_ += std::string(x.text.p, x.text.n);
    o_ += ')';
}
