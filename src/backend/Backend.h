#pragma once

#include "../Abi.h"
#include "../Ast.h"
#include "../Type.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Source;

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Program &program) = 0;

    virtual void setLineSource(const Source *, const std::string &) {}

    // **How hard to optimize**, 0 meaning not at all. Only a code generator
    // with an instruction IR in front of its spelling does anything with it.
    virtual void setOptimize(int) {}
};

enum class Segment { Code, Const, ConstRelocated, Data, Bss };

Segment segmentFor(const Global &g);

class Backend {
public:
    virtual ~Backend() = default;

    virtual const char *name() const = 0;
    virtual const Target &target() const = 0;
    virtual const Abi &abi() const = 0;

    // **`gnuAsm` is passed, not looked up.**
    virtual std::unique_ptr<CodeGen> codegen(std::ostream &sink,
                                             bool gnuAsm) const = 0;
    virtual bool emits() const = 0;

    virtual bool emitsLineTable(bool gnuAsm) const { (void) gnuAsm; return false; }

    virtual const char *const *identityMacros() const = 0;
};

std::vector<std::pair<std::string, std::string> > predefinedMacros(const Backend &b);

const Backend *findBackend(const std::string &name);
const Backend &defaultBackend();
std::string backendNames();
