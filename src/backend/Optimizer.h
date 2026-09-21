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
};

// **One of these per code generator, and so per file** - the driver compiles
// files on a thread pool, and nothing here is shared between two of them.
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

    void add(IrIns &&i);
    // **The peephole pass over one run.** Adjacent pairs only.
    void peephole();
    void replay();
};
