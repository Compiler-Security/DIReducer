#include "CFG/Func.h"
#include <cxxabi.h> // for __cxa_demangle, abi
#include <stdlib.h> // for free
#include <string>
#include <utility>
#include "CFG/AsmCFG.h"       // for AsmCFG
#include "CFG/AsmStatement.h" // for insnToString
#include "CFG/Utils.h"
#include "CFG/Var.h" // for Var
#include "Fig/assert.hpp"
#include "Fig/panic.hpp"
#include "nlohmann/json.hpp" // for basic_json

namespace CFG {
    /// Ref: DWARF5.pdf p52
    Func::Func(std::string &dwarfName, uint64_t low_pc, uint64_t high_pc) :
        type(FUNC_TYPE::COMMON), dwarfName(dwarfName), low_pc(low_pc), high_pc(high_pc + low_pc), size(high_pc) {
        this->module        = 0;
        this->insnBuffer    = nullptr;
        this->numOfInsn     = 0;
        this->instructions  = nullptr;
        this->demangledName = CFG::get_demangled_name(dwarfName);
    }

    Func::Func(const char *name, std::vector<Var> &&varList, VTypePointer retVType) :
        type(FUNC_TYPE::DYNAMIC), dwarfName(name), variableList(std::move(varList)), retVType(retVType) {
        this->module        = 0;
        this->insnBuffer    = nullptr;
        this->numOfInsn     = 0;
        this->instructions  = nullptr;
        this->demangledName = CFG::get_demangled_name(dwarfName);
    }

    std::string Func::UniqueName() const {
        return this->dwarfName + "@" + std::to_string(this->low_pc);
    }

    bool Func::isContainedData() const {
        return !(this->insnBuffer == nullptr);
    }

    bool Func::isParsed() const {
        return !(this->instructions == nullptr);
    }

    bool Func::hasCFG() const {
        return this->cfg.has_value();
    }

    void Func::setELFBuffer(const std::shared_ptr<char[]> &buffer) {
        this->elfBuffer = buffer;
    }

    void Func::setInsnbuffer(char *buffer) {
        this->insnBuffer = buffer;
    }

    void Func::disasm() {
        FIG_ASSERT(this->isContainedData(), "Can not disamsbly Without Data.");
        if(cs_open(CS_ARCH_X86, CS_MODE_64, &(this->handle)) != CS_ERR_OK)
            FIG_PANIC("Fail to disamsbly.");

        // detail mode
        cs_option(this->handle, CS_OPT_DETAIL, CS_OPT_ON);
        this->numOfInsn = cs_disasm(
            this->handle, reinterpret_cast<uint8_t *>(this->insnBuffer),
            this->size, this->low_pc, 0, &(this->instructions));
    }

    void Func::constructCFG() {
        FIG_ASSERT(this->isParsed());
        FIG_ASSERT(!this->hasCFG());
        FIG_ASSERT(this->elfBuffer.get() != nullptr);

        this->cfg.emplace(this->dwarfName, this->instructions, this->numOfInsn, this->elfBuffer);
    }

    nlohmann::json Func::toJson(bool withHeader = false, bool withInsns = false, bool withVars = false) const {
        nlohmann::json content = {
            {"module", this->module},
            {"dwarf_name", this->dwarfName},
            {"demangled_name", this->demangledName},
            {"unique_name", this->UniqueName()},
            {"size", this->size},
            {"low_pc", this->low_pc},
            {"high_pc", this->high_pc},
            {"num_of_insn", this->numOfInsn},
            {"returnType", this->retVType->name},
            {"Initialized", ((this->isContainedData()) ? "true" : "false")},
            {"Decompiled", ((this->isParsed()) ? "true" : "false")},
        };

        if(this->isParsed() && withInsns) {
            std::vector<std::string> asm_list(this->numOfInsn);
            for(uint32_t i = 0; i < this->numOfInsn; i++) {
                asm_list[i] = insnToString(this->instructions[i]);
            }
            content["Asm"] = nlohmann::json(asm_list);
        }

        if(withVars) {
            content["Vars"] = nlohmann::json::array();
            for(auto &v : this->variableList) {
                content["Vars"].push_back(v.toJson());
            }
            for(auto &p : this->parameterList) {
                content["Vars"].push_back(p.toJson());
            }
        }

        content["Blocks"] = nlohmann::json::array();
        for(const auto &[_, block] : this->cfg->blocks) {
            if(block->type != BLOCK_TYPE::COMMON_BLOCK)
                continue;

            uint64_t blockStart   = block->statements.front().address;
            uint64_t blockEnd     = block->statements.back().address;
            std::string blockInfo = "0x" + dec_to_hex(blockStart) + " - 0x" + dec_to_hex(blockEnd) + " (" + std::to_string(block->statements.size()) + " insns)";
            content["Blocks"].push_back(blockInfo);
        }

        if(withHeader)
            return {{"function", content}};
        else
            return content;
    }

} // namespace CFG