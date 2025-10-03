#include <stdint.h>            // for uint32_t, int32_t, int64_t
#include <memory>              // for shared_ptr, __shared_ptr_access
#include <optional>            // for optional
#include <set>                 // for set
#include <unordered_map>       // for unordered_map
#include <vector>              // for vector
#include "CFG/AsmBlock.h"      // for AsmBlock, BLOCK_TYPE
#include "CFG/AsmCFG.h"        // for AsmCFG
#include "CFG/Type.h"          // for VTypePointer
#include "CFG/TypeFactory.h"   // for VTypeFactory
#include "CFG/Var.h"           // for Var
#include "Core/Analysis.h"     // for TIAnalysis
#include "Core/TIContext.h"    // for TIContext
#include "Core/TIOperand.h"    // for TIOperandFactory, TIOperandRef
#include "capstone/capstone.h" // for cs_insn
#include "capstone/x86.h"      // for x86_reg, cs_x86_op, x86_insn, x86_op_...

namespace SA {
    /// @brief Infer Type of Function Parameters
    void TIAnalysis::parameterInference() {
        auto entryBlock = this->func->cfg.value().getEntryBlock();
        std::set<uint32_t> snVisited{};
        this->parameterIterativeExecution(snVisited, entryBlock);
    }

    void TIAnalysis::parameterIterativeExecution(std::set<uint32_t> &snVisited, const std::shared_ptr<CFG::AsmBlock> &block) {
        if(snVisited.contains(block->sn))
            return;
        snVisited.insert(block->sn);

        this->parameterInferenceInBlock(block);
        for(const auto &it : block->getSuccsExceptCall()) {
            this->parameterIterativeExecution(snVisited, it);
        }
    }

    void TIAnalysis::parameterInferenceInBlock(const std::shared_ptr<CFG::AsmBlock> &block) {
        if(block->type == CFG::BLOCK_TYPE::ENTRY_BLOCK || block->type == CFG::BLOCK_TYPE::EXIT_BLOCK)
            return;

        auto &inContext = this->inBlockContexts[block->sn];
        for(int32_t i = 0; i < static_cast<int32_t>(block->statements.size()); i++) {
            auto &insn = block->statements[i];
            if(i == 0) {
                this->parameterInferenceInStatement(inContext, insn);
            } else {
                auto &insnINContext = this->blockInsnContexts[block->sn][i - 1];
                this->parameterInferenceInStatement(insnINContext, insn);
            }
        }
    }

    void TIAnalysis::parameterInferenceInStatement(TIContext &insnINContext, const cs_insn &insn) {
        if(insn.id != X86_INS_CALL)
            return;

        const auto &x86Detail     = insn.detail->x86;
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef callTargetOP = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        if(callTargetOP->type != X86_OP_IMM)
            return;
        int64_t callTargetAddr = callTargetOP->detail.imm;
        if(!this->customFuncMap->contains(callTargetAddr))
            return;

        auto isSystemVABIIntegerClass = [](CFG::VTypePointer vType) -> bool {
            return vType->size <= 8 && (CFG::VTypeFactory::isPointerType(vType) || CFG::VTypeFactory::isBoolType(vType) || CFG::VTypeFactory::isIntegerType(vType));
        };
        auto isSystemVABISSEClass = [](CFG::VTypePointer vType) -> bool {
            return vType->size <= 8 && (CFG::VTypeFactory::isFloatType(vType));
        };

        // ref: System-V-ABI-AMD64.pdf, link: https://gitlab.com/x86-psABIs/x86-psABI/
        static x86_reg PTIIntegerClassRegisters[6][2] = {
            {X86_REG_EDI, X86_REG_RDI},
            {X86_REG_ESI, X86_REG_RSI},
            {X86_REG_EDX, X86_REG_RDX},
            {X86_REG_ECX, X86_REG_RCX},
            {X86_REG_R8D, X86_REG_R8},
            {X86_REG_R9D, X86_REG_R9}};
        static x86_reg PTISSEClassRegisters[8] = {
            X86_REG_XMM0,
            X86_REG_XMM1,
            X86_REG_XMM2,
            X86_REG_XMM3,
            X86_REG_XMM4,
            X86_REG_XMM5,
            X86_REG_XMM6,
            X86_REG_XMM7,
        };
        uint32_t IntegerClassIndex = 0;
        uint32_t SSEClassIndex     = 0;
        auto targetFunc            = (*this->customFuncMap.get())[callTargetAddr];
        for(auto &p : targetFunc->parameterList) {
            auto vType = p.vType;
            if(isSystemVABIIntegerClass(vType) && IntegerClassIndex < 6) {
                uint32_t bitwidthFlag    = vType->size > 4 ? 1 : 0;
                x86_reg targetReg        = PTIIntegerClassRegisters[IntegerClassIndex][bitwidthFlag];
                TIOperandRef targetRegOP = this->OPFactory.getRegOPByx86Reg(targetReg);
                if(insnINContext.contains(targetRegOP) && insnINContext[targetRegOP] == p.vType) {
                    if(p.vType->typeTag == CFG::TYPETAG::POINTER) {
                        p.isInfered = true;
                    } else {
                        p.isInfered = true;
                    }
                }
                // next Integer Class register
                IntegerClassIndex++;
            } else if(isSystemVABISSEClass(vType) && SSEClassIndex < 8) {
                x86_reg targetReg        = PTISSEClassRegisters[SSEClassIndex];
                TIOperandRef targetRegOP = this->OPFactory.getRegOPByx86Reg(targetReg);
                if(insnINContext.contains(targetRegOP) && insnINContext[targetRegOP] == p.vType) {
                    p.isInfered = true;
                }
                // next SSE Class register
                SSEClassIndex++;
            } else {
                // do nothing
            }
        }
    }
} // namespace SA