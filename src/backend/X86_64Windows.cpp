#include "X86_64Windows.h"
#include "X86_64Linux.h"
#include "Masm.h"

#include <cstdio>
#include <cstdlib>

int WindowsX86_64Target::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                       return 1;
    case Kind::Bool:                                       return 1;
    case Kind::Char: case Kind::SChar: case Kind::UChar:   return 1;
    case Kind::Short: case Kind::UShort:                   return 2;
    case Kind::WChar:                                      return sizeOf(wcharType());
    case Kind::Int: case Kind::UInt:                       return 4;
    case Kind::Long: case Kind::ULong:                     return 4;
    case Kind::LongLong: case Kind::ULongLong:             return 8;
    case Kind::Float:                                      return 4;
    case Kind::Double:                                     return 8;

    case Kind::LongDouble:                                 return 8;
    case Kind::Pointer: case Kind::NullPtr:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

int WindowsX86_64Target::alignOf(Kind k) const { return sizeOf(k); }

static const char *const kArgRegs[] = { "%rcx", "%rdx", "%r8", "%r9" };
static const char *const kSseRegs[] = { "%xmm0", "%xmm1", "%xmm2", "%xmm3" };

// Microsoft x64. The two that are only true here are `positional` - the third
// argument is the third register whichever class it is - and the 32 bytes of
// shadow space the caller leaves for the callee to spill into.
static Abi microsoft() {
    Abi a;
    a.intRegs = kArgRegs;               a.intCount = 4;
    a.sseRegs = kSseRegs;               a.sseCount = 4;
    a.positional = true;
    a.shadowBytes = 32;
    a.structReturnLimit = 8;
    a.aggregatesByReference = true;
    a.stringRegsCalleeSaved = true;
    a.scratch = "%r10";                 a.scratch32 = "%r10d";
    return a;
}

static const Abi kMsAbi = microsoft();

const Abi &X86_64WindowsBackend::abi() const { return kMsAbi; }

static const char *const kWindowsMacros[] = {
    "__x86_64__=1", "__x86_64=1", "__amd64__=1", "__amd64=1",
    "_WIN32=1", "_WIN64=1", "__llp64__=1", nullptr,
};
const char *const *X86_64WindowsBackend::identityMacros() const { return kWindowsMacros; }

bool X86_64WindowsBackend::emitsLineTable(bool gnuAsm) const { return gnuAsm; }

std::unique_ptr<CodeGen> X86_64WindowsBackend::codegen(std::ostream &sink,
                                                      bool gnuAsm) const {
    if (gnuAsm)
        return std::unique_ptr<CodeGen>(new X86_64Linux(sink, target_, kMsAbi));
    return std::unique_ptr<CodeGen>(new MasmCodeGen(sink, target_, kMsAbi));
}
