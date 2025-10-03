#include "CFG/Var.h"
#include "CFG/Type.h"            // for VTypePointer
#include "CFG/Utils.h"           // for dec_to_hex
#include "Fig/panic.hpp"         // for FIG_PANIC
#include "nlohmann/json_fwd.hpp" // for basic_json

namespace CFG {
    VarAddr::VarAddr() :
        type(ADDR_TYPE::NONE), globalAddr(0), CFAOffset(0), reg(0) {
    }

    nlohmann::json VarAddr::toJson() const {
        switch(this->type) {
        case ADDR_TYPE::GLOBAL:
            return {{"type", "GLOBAL"}, {"global_addr", "0x" + dec_to_hex(this->globalAddr)}};
        case ADDR_TYPE::STACK:
            return {{"type", "STACK"}, {"stack_offset", this->CFAOffset}};
        case ADDR_TYPE::REG:
            return {{"type", "REG"}, {"reg", this->reg}};
        default: FIG_PANIC("var_addr's type is NONE!");
        }
    }

    Var::Var() :
        varId(0), name(), addr(), vType(nullptr),
        low_pc(0), high_pc(0),
        isParameter(false), isUsed(false), isInfered(false) {
    }

    Var::Var(const char *name, VTypePointer vType) :
        varId(0), name(name), addr(), vType(vType),
        isParameter(true), isUsed(false), isInfered(false) {
    }

    nlohmann::json Var::toJson() const {
        nlohmann::json content = {
            {"name", (this->name.empty() ? std::string("UnknownVariableName") : this->name)},
            {"var_id", this->varId},
            {"is_parameter", this->isParameter},
            {"low_pc", this->low_pc},
            {"hign_pc", this->high_pc},
        };
        content["type"] = this->vType == nullptr ? "UnknownVariableType" : this->vType->toJson();
        content["addr"] = this->addr.toJson();

        return content;
    }
} // namespace CFG