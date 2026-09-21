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

// **The general registers whose 64-bit self-move is a true no-op.** Moving
// a 32-bit register onto itself zero-extends into the whole register, and a
// 16- or 8-bit one is a partial write; only these names may be dropped.
static bool is64Gpr(const std::string &r) {
    static const char *const names[] = {
        "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
        "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
    };
    for (const char *n : names)
        if (r == n) return true;
    return false;
}

// A run ends at anything that can leave it other than by falling through.
static bool endsRun(const std::string &m) {
    return (!m.empty() && m[0] == 'j') || m == "call" || m == "ret";
}

// **The 64-bit move is spelled `mov` by the walker and `movq` in a few
// places**; with a 64-bit general register on one side they are the same
// instruction, and only then is the width certain.
static bool isMov64(const IrIns &i) {
    return (i.m == "mov" || i.m == "movq") && i.operands == 2;
}

static bool isRegToMem(const IrIns &i) {
    return isMov64(i) && i.a.kind == Op::Reg && i.b.kind == Op::Mem &&
           is64Gpr(i.a.text);
}

static bool isMemToReg(const IrIns &i) {
    return isMov64(i) && i.a.kind == Op::Mem && i.b.kind == Op::Reg &&
           is64Gpr(i.b.text);
}

// A push or pop of a 64-bit general register other than the two that address
// the stack, which is every one the walker's expression stack uses.
static bool isStackOp(const IrIns &i, const char *m) {
    return i.m == m && i.operands == 1 && i.a.kind == Op::Reg &&
           is64Gpr(i.a.text) && i.a.text != "%rsp" && i.a.text != "%rbp";
}

void Optimizer::add(IrIns &&i) {
    bool ends = endsRun(i.m);
    run_.push_back(std::move(i));
    if (ends) flush();
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

// **Each pattern is between an instruction and the last one kept**, which is
// what lets a chain fall: a store and two reloads lose both reloads. The first
// three never fire on what the walker writes today; the fourth is its idiom.
void Optimizer::peephole() {
    std::vector<IrIns> kept;
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
        if (!kept.empty() && isStackOp(i, "pop") && isStackOp(kept.back(), "push")) {
            IrIns &p = kept.back();
            if (p.a.text == i.a.text) {
                kept.pop_back();
                removed_ += 2;
            } else {
                p.m = "mov";
                p.operands = 2;
                p.b = i.a;
                removed_++;
            }
            continue;
        }
        kept.push_back(std::move(i));
    }
    run_.swap(kept);
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

void Optimizer::flush() {
    if (run_.empty()) return;
    if (level_ >= 1) peephole();
    replay();
}

// Everything else ends the run and goes through in order.
void Optimizer::defLabel(const std::string &l) { flush(); under_->defLabel(l); }
void Optimizer::functionBegin(const std::string &name, bool exported, bool mergeable) {
    flush(); under_->functionBegin(name, exported, mergeable);
}
void Optimizer::prologue(int frameSize, const std::string &lsda) {
    flush(); under_->prologue(frameSize, lsda);
}
void Optimizer::functionEnd(const std::string &name) { flush(); under_->functionEnd(name); }
void Optimizer::fileEntry(int n, const std::string &name) { flush(); under_->fileEntry(n, name); }
void Optimizer::location(int file, int line, int column) {
    flush(); under_->location(file, line, column);
}
void Optimizer::predefine(const std::vector<std::string> &names) { flush(); under_->predefine(names); }
void Optimizer::preamble(std::ostream &o) { flush(); under_->preamble(o); }
void Optimizer::postamble(std::ostream &o) { flush(); under_->postamble(o); }
void Optimizer::globl(const std::string &name) { flush(); under_->globl(name); }
void Optimizer::weakDefinition(const std::string &name) { flush(); under_->weakDefinition(name); }
void Optimizer::textSection() { flush(); under_->textSection(); }
void Optimizer::rodataSection() { flush(); under_->rodataSection(); }
void Optimizer::dataSection() { flush(); under_->dataSection(); }
void Optimizer::bssSection() { flush(); under_->bssSection(); }
void Optimizer::objectType(const std::string &name) { flush(); under_->objectType(name); }
void Optimizer::objectSize(const std::string &name, int size) { flush(); under_->objectSize(name, size); }
void Optimizer::align(int n) { flush(); under_->align(n); }
void Optimizer::zero(int n) { flush(); under_->zero(n); }
void Optimizer::dataInt(int size, long long v) { flush(); under_->dataInt(size, v); }
void Optimizer::dataSym(const std::string &sym, long long off) { flush(); under_->dataSym(sym, off); }
void Optimizer::noteHasEh(bool yes) { flush(); under_->noteHasEh(yes); }
void Optimizer::initialiserEntry(const std::string &fn, bool dsoHandle) {
    flush(); under_->initialiserEntry(fn, dsoHandle);
}
std::string Optimizer::labelText(const std::string &l) const { return under_->labelText(l); }
void Optimizer::dataBytes(const std::string &bytes) { flush(); under_->dataBytes(bytes); }
