#ifndef LIBCFG_VAR_H
#define LIBCFG_VAR_H

#include <cstdint>               // for uint64_t, int64_t, uint16_t, uint32_t
#include <string>                // for string
#include "Type.h"                // for VTypePointer
#include "nlohmann/json_fwd.hpp" // for json

namespace CFG {
    struct VarAddr;
    struct Var;

    enum class ADDR_TYPE {
        GLOBAL,
        STACK,
        REG,
        NONE,
    };
} // namespace CFG

struct CFG::VarAddr {
public:
    VarAddr();

    nlohmann::json toJson() const;

public:
    CFG::ADDR_TYPE type;
    uint64_t globalAddr;
    int64_t CFAOffset;
    uint16_t reg;
};

struct CFG::Var {
public:
    Var();
    Var(const char *name, VTypePointer vType);
    nlohmann::json toJson() const;

public:
    // unique id, depend on offset of DIE
    uint32_t varId;
    std::string name;
    CFG::VarAddr addr;
    VTypePointer vType;
    uint64_t low_pc;
    uint64_t high_pc;
    bool isParameter;
    bool isUsed;
    bool isInfered;
};

#endif