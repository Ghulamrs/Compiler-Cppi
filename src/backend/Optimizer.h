#pragma once

#include "Spelling.h"

#include <string>
#include <vector>

// **An instruction IR between the walker and the spelling.** The walker's
// instructions are kept in owned form until a straight-line run ends, rewritten,
// and only then handed to the real spelling; an Op's Str is a view, so it is copied.
struct IrOp {
    Op::Kind kind = Op::Reg;
    std::string text;
    long long disp = 0;
    bool hasDisp = false;
    unsigned long long uimm = 0;
    bool immNeg = false;
    bool immNumeric = false;

    static IrOp from(const Op &o);
    Op view() const;
    bool same(const IrOp &o) const;
};

struct IrIns {
    std::string m;
    int operands = 0;
    IrOp a, b;
    // Set by a pass; the run is compacted before it is replayed.
    bool dead = false;
};

// **What one instruction does to the machine**, as far as the passes need to
// know: which registers it reads, which it writes whole, which it writes in
// part - a part write keeps the rest, so it is a use as well - and the flags.

// Registers are canonical: every width of a name maps to the one 64-bit
// register it lives in, because a write to %eax is a write to %rax and a
// read of %al is a read of %rax.

// Bit i of a set is canonical register i: 0 rax, 1 rbx, 2 rcx, 3 rdx, 4 rsi,
// 5 rdi, 6 rbp, 7 rsp, 8-15 r8-r15, 16-31 xmm0-xmm15.
struct IrSem {
    enum Class {
        Unknown,  // a mnemonic not in the table: reads and writes everything
        Move,     // a is read, b is written and not read
        Rmw,      // a is read, b is read and written
        Cmp,      // both read, flags written
        Set,      // a written in part from the flags
        Unary,    // a read and written
        Push, Pop,
        Cqo, Cdq, Cltq,
        Div,      // rax:rdx in and out, a read
        X87,      // only memory and the x87 stack
        Call, Jump, Ret, Leave, Nop,
    };
    Class cls = Unknown;
    unsigned use = 0;        // read, at any width, base registers included
    unsigned def = 0;        // written whole, and not read
    unsigned part = 0;       // written in part - also in `use`
    // A read that a rename may not touch: an implicit operand, the shift
    // count, the destination of a read-modify-write.
    unsigned fixed = 0;
    bool flagsUse = false;
    bool flagsDef = false;
    bool memWrite = false;
    // The register a Move or Pop writes whole, or -1; and its width.
    int dstReg = -1;
    int dstWidth = 0;
};

// **One of these per code generator, and so per file** - the driver compiles
// files on a thread pool, and nothing here is shared between two of them:
// every table below is a constant, every vector a member.
class Optimizer final : public Spelling {
public:
    Optimizer() = default;

    // Interpose in front of `under`; every call is forwarded there.
    void wrap(Spelling *under, int level) { under_ = under; level_ = level; }
    bool active() const { return under_ != nullptr; }

    // Write out what is buffered - called before the output text is read or cut.
    void flush();

    // How many instructions the passes removed, for anyone measuring.
    unsigned long removed() const { return removed_; }

    void ins(const std::string &m) override;
    void ins(const std::string &m, const Op &a) override;
    void ins(const std::string &m, const Op &a, const Op &b) override;

    void defLabel(const std::string &l) override;
    void functionBegin(const std::string &name, bool exported,
                       bool mergeable = false) override;
    void prologue(int frameSize, const std::string &lsda) override;
    void functionEnd(const std::string &name) override;
    void fileEntry(int n, const std::string &name) override;
    void location(int file, int line, int column) override;
    void predefine(const std::vector<std::string> &names) override;
    void preamble(std::ostream &o) override;
    void postamble(std::ostream &o) override;
    void globl(const std::string &name) override;
    void weakDefinition(const std::string &name) override;
    void textSection() override;
    void rodataSection() override;
    void dataSection() override;
    void bssSection() override;
    void objectType(const std::string &name) override;
    void objectSize(const std::string &name, int size) override;
    void align(int n) override;
    void zero(int n) override;
    void dataInt(int size, long long v) override;
    void dataSym(const std::string &sym, long long off) override;
    void noteHasEh(bool yes) override;
    void initialiserEntry(const std::string &fn, bool dsoHandle) override;
    std::string labelText(const std::string &l) const override;
    void dataBytes(const std::string &bytes) override;

private:
    Spelling *under_ = nullptr;
    int level_ = 0;
    // The straight-line run being collected. A label, a branch, or anything
    // that is not an instruction ends it.
    std::vector<IrIns> run_;
    unsigned long removed_ = 0;
    // True after a jmp or a ret with no label since: nothing reaches there.
    bool unreachable_ = false;

    // Per-run scratch, kept as members so the storage is reused run to run.
    std::vector<IrSem> sems_;
    std::vector<unsigned> liveOut_;
    std::vector<unsigned char> flagsLiveOut_;
    std::vector<IrIns> kept_;

    void add(IrIns &&i);
    void endRun();
    // Everything that is not an instruction: writes the run out, and what
    // follows may be a label, so is reachable again.
    void interrupt();
    void optimize();
    void replay();

    // The passes. Each works on run_ and marks what it removes dead;
    // compact() drops those, analyse() recomputes sems_ and the liveness.
    void analyse();
    void compact();
    void peephole();
    bool dropExtensions();
    bool fuseLeas();
    bool pairStack();
    bool retargetDefs();
    bool propagateCopies();
    void shorten();
};
