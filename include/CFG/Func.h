#ifndef LIBCFG_FUNC_H
#define LIBCFG_FUNC_H

#include <cstdint>               // for uint64_t, uint16_t, uint32_t
#include <memory>                // for shared_ptr
#include <optional>              // for optional
#include <string>                // for string
#include <vector>                // for vector
#include "AsmCFG.h"              // for AsmCFG
#include "Type.h"                // for VTypePointer
#include "Var.h"                 // for Var
#include "capstone/capstone.h"   // for cs_insn, csh
#include "nlohmann/json_fwd.hpp" // for json

namespace CFG {
    struct Func;
    enum class FUNC_TYPE {
        DYNAMIC,
        COMMON
    };
} // namespace CFG

struct CFG::Func {
public:
    uint16_t module;
    FUNC_TYPE type;
    std::string dwarfName;
    std::string demangledName;
    uint64_t low_pc;
    uint64_t high_pc;
    uint64_t size;
    std::shared_ptr<char[]> elfBuffer;

public:
    // parameters
    std::vector<Var> parameterList;
    // variables, except parameters
    std::vector<Var> variableList;
    // return type
    VTypePointer retVType;
    // cfg of func
    std::optional<CFG::AsmCFG> cfg;
    uint32_t numOfInsn;

private:
    csh handle;
    cs_insn *instructions;
    char *insnBuffer;

public:
    Func() = delete;
    Func(std::string &name, uint64_t low_pc, uint64_t high_pc);
    Func(const char *name, std::vector<Var> &&varList, VTypePointer retVType);

    std::string UniqueName() const;
    bool isContainedData() const;
    bool isParsed() const;
    bool hasCFG() const;
    void setELFBuffer(const std::shared_ptr<char[]> &buffer);
    void setInsnbuffer(char *buffer);
    void disasm();
    void constructCFG();

    nlohmann::json toJson(bool withHeader, bool withInsns, bool withVars) const;
};

#endif