#include <cstdint>               // for uint32_t, int32_t, int64_t
#include <memory>                // for shared_ptr, __shared_ptr_access, allo...
#include <optional>              // for optional
#include <set>                   // for set
#include <unordered_map>         // for unordered_map, operator==
#include <vector>                // for vector
#include "CFG/AsmBlock.h"        // for AsmBlock, BLOCK_TYPE
#include "CFG/AsmCFG.h"          // for AsmCFG
#include "CFG/AsmStatement.h"    // for insnToString
#include "CFG/Type.h"            // for BASETYPE, VTypePointer, TYPETAG
#include "CFG/TypeFactory.h"     // for VTypeFactory
#include "Fig/assert.hpp"        // for FIG_ASSERT
#include "Fig/debug.hpp"         // for FIG_DEBUG
#include "Fig/panic.hpp"         // for FIG_PANIC
#include "Core/Analysis.h"       // for TIAnalysis, CFA
#include "Core/TIConfigure.h"    // for TIConfigure
#include "Core/TIContext.h"      // for TIContext
#include "Core/TIOperand.h"      // for TIOperandFactory, TIOperandRef
#include "Core/TypeCalculator.h" // for TypeCalculator
#include "capstone/capstone.h"   // for cs_insn
#include "capstone/x86.h"        // for x86_insn, x86_reg, x86_op_type, cs_x8...

namespace SA {
    void TIAnalysis::forwardInference() {
        auto entryBlock = this->func->cfg.value().getEntryBlock();
        std::set<uint32_t> snVisited{};
        this->forwardIterativeExecution(snVisited, entryBlock);
    }

    void TIAnalysis::forwardIterativeExecution(std::set<uint32_t> &snVisited, std::shared_ptr<CFG::AsmBlock> &block) {
        if(snVisited.contains(block->sn))
            return;
        snVisited.insert(block->sn);

        this->forwardInferenceInBlock(block);
        for(auto &it : block->getSuccsExceptCall()) {
            this->forwardIterativeExecution(snVisited, it);
        }
    }

    bool TIAnalysis::forwardInferenceInBlock(const std::shared_ptr<CFG::AsmBlock> &block) {
        if(block->type == CFG::BLOCK_TYPE::ENTRY_BLOCK || block->type == CFG::BLOCK_TYPE::EXIT_BLOCK) {
            this->outBlockContexts[block->sn] = this->inBlockContexts[block->sn];
            return false;
        }

        auto &inContext = this->inBlockContexts[block->sn];
        TIContext baseContext;
        for(auto &fromBlock : block->getPredsExceptCall()) {
            const auto &fromContext = this->outBlockContexts[fromBlock->sn];
            this->unionTIContext(baseContext, fromContext);
        }
        inContext = baseContext;

        for(int32_t i = 0; i < static_cast<int32_t>(block->statements.size()); i++) {
            auto &insn        = block->statements[i];
            auto &insnContext = this->blockInsnContexts[block->sn][i];
            auto &insnCFA     = this->blockInsnCFA[block->sn][i];
            if(i == 0) {
                this->forwardInferenceInStatement(inContext, insnContext, insnCFA, insn);
            } else {
                this->forwardInferenceInStatement(this->blockInsnContexts[block->sn][i - 1], insnContext, insnCFA, insn);
            }
        }

        auto &outContext      = this->outBlockContexts[block->sn];
        auto &lastInsnContext = this->blockInsnContexts[block->sn][block->statements.size() - 1];
        if(lastInsnContext == outContext) {
            return false;
        } else {
            outContext = lastInsnContext;
            return true;
        }
    }

    bool TIAnalysis::forwardInferenceInStatement(const TIContext &insnINContext, TIContext &insnOUTContext, const CFA &cfa, const cs_insn &insn) {
        TIContext ctx = insnINContext;
        // reset rip
        this->insnSemRemoveReg(ctx, X86_REG_RIP);
        const auto &x86Detail = insn.detail->x86;
        const auto operands   = x86Detail.operands;
        switch(insn.id) {
        case X86_INS_JMP:
        case X86_INS_LJMP:
        case X86_INS_JAE:
        case X86_INS_JA:
        case X86_INS_JBE:
        case X86_INS_JB:
        case X86_INS_JCXZ:
        case X86_INS_JECXZ:
        case X86_INS_JE:
        case X86_INS_JGE:
        case X86_INS_JG:
        case X86_INS_JLE:
        case X86_INS_JL:
        case X86_INS_JNE:
        case X86_INS_JNO:
        case X86_INS_JNP:
        case X86_INS_JNS:
        case X86_INS_JO:
        case X86_INS_JP:
        case X86_INS_JRCXZ:
        case X86_INS_JS: this->insnSemOP1NOP(insn); break;
        case X86_INS_CALL: this->insnSemForwardOP1Call(ctx, insn); break;
        case X86_INS_MOVZX: this->insnSemForwardOP2MOVZX(ctx, insn); break;
        case X86_INS_MOVSX: this->insnSemForwardOP2MOVSX(ctx, insn); break;
        case X86_INS_MOVSXD: this->insnSemForwardOP2MOVSXD(ctx, insn); break;
        case X86_INS_MOVDQA:
        case X86_INS_MOVABS:
        case X86_INS_MOVAPS:
        case X86_INS_MOVAPD:
        case X86_INS_MOVSS:
        case X86_INS_MOVSD:
        case X86_INS_MOVSQ:
        case X86_INS_MOVQ:
        case X86_INS_MOVD:
        case X86_INS_MOV: this->insnSemForwardOP2MOVFamily(ctx, insn, cfa); break;
        case X86_INS_CMOVE:
        case X86_INS_CMOVNE:
        case X86_INS_CMOVS:
        case X86_INS_CMOVNS:
        case X86_INS_CMOVG:
        case X86_INS_CMOVGE:
        case X86_INS_CMOVL:
        case X86_INS_CMOVLE:
        case X86_INS_CMOVA:
        case X86_INS_CMOVAE:
        case X86_INS_CMOVB:
        case X86_INS_CMOVBE: this->insnSemForwardOP2MOVFamily(ctx, insn, cfa); break;
        case X86_INS_LEA: this->insnSemForwardOP2LEA(ctx, insn, cfa); break;
        case X86_INS_NOP:
        case X86_INS_RET:
        case X86_INS_LEAVE:
        case X86_INS_ENDBR64: this->insnSemOP0NOP(insn); break;
        case X86_INS_PUSH:
        case X86_INS_POP: this->insnSemOP1NOP(insn); break;
        case X86_INS_CQO: {
            this->insnSemRemoveReg(ctx, X86_REG_DL);
            this->insnSemRemoveReg(ctx, X86_REG_DX);
            this->insnSemRemoveReg(ctx, X86_REG_EDX);
            this->insnSemRemoveReg(ctx, X86_REG_RDX);
            break;
        }
        case X86_INS_CDQ: {
            this->insnSemRemoveReg(ctx, X86_REG_DL);
            this->insnSemRemoveReg(ctx, X86_REG_DX);
            this->insnSemRemoveReg(ctx, X86_REG_EDX);
            this->insnSemRemoveReg(ctx, X86_REG_RDX);
            break;
        }
        case X86_INS_CBW: {
            this->insnSemRemoveReg(ctx, X86_REG_AH);
            cs_x86_op csX86OP;
            csX86OP.type     = X86_OP_REG;
            csX86OP.reg      = X86_REG_AX;
            csX86OP.size     = 2;
            auto typeInferOP = this->OPFactory.getTypeInferOPByX86OP(csX86OP);
            ctx.insert({typeInferOP, CFG::VTypeFactory::baseVType.bt_int16});
            break;
        }
        case X86_INS_CWD: {
            this->insnSemRemoveReg(ctx, X86_REG_DL);
            this->insnSemRemoveReg(ctx, X86_REG_DX);
            this->insnSemRemoveReg(ctx, X86_REG_EDX);
            this->insnSemRemoveReg(ctx, X86_REG_RDX);
            break;
        }
        case X86_INS_CWDE: {
            this->insnSemRemoveReg(ctx, X86_REG_AL);
            this->insnSemRemoveReg(ctx, X86_REG_AX);
            this->insnSemRemoveReg(ctx, X86_REG_EAX);
            this->insnSemRemoveReg(ctx, X86_REG_RAX);
            cs_x86_op csX86OP;
            csX86OP.type     = X86_OP_REG;
            csX86OP.reg      = X86_REG_AX;
            csX86OP.size     = 2;
            auto typeInferOP = this->OPFactory.getTypeInferOPByX86OP(csX86OP);
            ctx.insert({typeInferOP, CFG::VTypeFactory::baseVType.bt_int16});
            break;
        }
        case X86_INS_CDQE: {
            this->insnSemRemoveReg(ctx, X86_REG_AL);
            this->insnSemRemoveReg(ctx, X86_REG_AX);
            this->insnSemRemoveReg(ctx, X86_REG_EAX);
            this->insnSemRemoveReg(ctx, X86_REG_RAX);
            cs_x86_op csX86OP;
            csX86OP.type     = X86_OP_REG;
            csX86OP.reg      = X86_REG_RAX;
            csX86OP.size     = 8;
            auto typeInferOP = this->OPFactory.getTypeInferOPByX86OP(csX86OP);
            ctx.insert({typeInferOP, CFG::VTypeFactory::baseVType.bt_int64});
            break;
        }
        case X86_INS_STOSQ: {
            this->insnSemForwardOP2MOVFamily(ctx, insn, cfa);
            break;
        }
        case X86_INS_MOVMSKPS:
        case X86_INS_MOVMSKPD: {
            if(x86Detail.op_count != 2)
                FIG_PANIC("In X86_INS_MOVMSKPD, op_count != 1");
            this->insnSemRemoveOP(ctx, this->OPFactory.getTypeInferOPByX86OP(operands[0]));
            break;
        }
        case X86_INS_FXAM: {
            this->insnSemOP0NOP(insn);
            break;
        }
        case X86_INS_FNSTSW:
        case X86_INS_FNSTCW:
        case X86_INS_FNSAVE:
        case X86_INS_FNSTENV: {
            if(x86Detail.op_count != 1)
                FIG_PANIC("In X86_INS_FNSTCW, op_count != 1");
            this->insnSemRemoveOP(ctx, this->OPFactory.getTypeInferOPByX86OP(operands[0]));
            break;
        }
        case X86_INS_FCHS:
        case X86_INS_FDIV:
        case X86_INS_FDIVP:
        case X86_INS_FDIVRP:
        case X86_INS_FMUL:
        case X86_INS_FMULP:
        case X86_INS_FSUB:
        case X86_INS_FSUBP:
        case X86_INS_FSUBRP:
        case X86_INS_FADD: {
            break;
        }
        case X86_INS_FLDZ:   // load 0
        case X86_INS_FLD1: { // load 1
            this->insnSemOP0NOP(insn);
            break;
        }
        case X86_INS_FLD: {
            this->insnSemOP1SetFloat(ctx, insn);
            break;
        }
        case X86_INS_FXCH: {
            this->insnSemOP2NOP(insn);
            break;
        }
        case X86_INS_FCOM:
        case X86_INS_FCOMI:
        case X86_INS_FUCOMPI:
        case X86_INS_FUCOMP:
        case X86_INS_FUCOMI: {
            this->insnSemOP1NOP(insn);
            break;
        }
        case X86_INS_FCOMPI: {
            this->insnSemOP1NOP(insn);
            break;
        }
        case X86_INS_FABS: {
            this->insnSemOP0NOP(insn);
            break;
        }
        case X86_INS_FST:
        case X86_INS_FSTP: {
            this->insnSemOP1SetFloat(ctx, insn);
            break;
        }
        case X86_INS_FLDCW: {
            this->insnSemOP1NOP(insn);
            break;
        }
        // float => int, flost stack -> mem
        case X86_INS_FISTTP:
        case X86_INS_FISTP: {
            this->insnSemOP1SetSignedInt(ctx, insn);
            break;
        }
        // int => float, mem -> flost stack
        case X86_INS_FILD: {
            this->insnSemOP1SetSignedInt(ctx, insn);
            break;
        }
        // int <=> float, reg <-> xmm reg
        case X86_INS_CVTSD2SS:
        case X86_INS_CVTSD2SI:
        case X86_INS_CVTSS2SD:
        case X86_INS_CVTSS2SI:
        case X86_INS_CVTSI2SD:
        case X86_INS_CVTSI2SS:
        case X86_INS_CVTPD2DQ:
        case X86_INS_CVTPD2PI:
        case X86_INS_CVTPD2PS:
        case X86_INS_CVTPI2PD:
        case X86_INS_CVTPI2PS:
        case X86_INS_CVTPS2DQ:
        case X86_INS_CVTPS2PD:
        case X86_INS_CVTPS2PI:
        case X86_INS_CVTTPD2DQ:
        case X86_INS_CVTTPD2PI:
        case X86_INS_CVTTPS2PI:
        case X86_INS_CVTTPS2DQ:
        case X86_INS_CVTTSD2SI:
        case X86_INS_CVTTSS2SI: {
            break;
        }
        case X86_INS_CMPXCHG:
        case X86_INS_VUCOMISD:
        case X86_INS_VUCOMISS:
        case X86_INS_COMISD:
        case X86_INS_COMISS:
        case X86_INS_UCOMISD:
        case X86_INS_UCOMISS:
        case X86_INS_CMPSW:
        case X86_INS_CMP: this->insnSemForwardOP2CMPFamily(ctx, insn, cfa); break;
        case X86_INS_TEST:
        case X86_INS_AND:
        case X86_INS_PAND:
        case X86_INS_ANDPD:
        case X86_INS_ANDPS:
        case X86_INS_ANDN:
        case X86_INS_PANDN:
        case X86_INS_ANDNPD:
        case X86_INS_ANDNPS:
        case X86_INS_XOR:
        case X86_INS_PXOR:
        case X86_INS_XORPD:
        case X86_INS_XORPS:
        case X86_INS_OR:
        case X86_INS_POR:
        case X86_INS_ORPD:
        case X86_INS_ORPS: {
            this->insnSemForwardOP2LOGICFamily(ctx, insn, cfa);
            break;
        }
        case X86_INS_NOT: this->insnSemOP1NOP(insn); break;
        case X86_INS_SETNS:
        case X86_INS_SETS:
        case X86_INS_SETNO:
        case X86_INS_SETO:
        case X86_INS_SETNP:
        case X86_INS_SETP:
        case X86_INS_SETNE:
        case X86_INS_SETE:
        case X86_INS_SETAE:
        case X86_INS_SETA:
        case X86_INS_SETLE:
        case X86_INS_SETL:
        case X86_INS_SETGE:
        case X86_INS_SETG:
        case X86_INS_SETBE:
        case X86_INS_SETB: {
            if(x86Detail.op_count != 1) FIG_PANIC("In sete, op_count != 1");
            if(operands[0].type != X86_OP_REG) FIG_PANIC("In sete, op is not reg!");
            this->insnSemRemoveReg(ctx, operands[0].reg);
            auto op0 = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
            ctx.insert({op0, CFG::VTypeFactory::baseVType.bt_bool});
            break;
        }
        case X86_INS_BSR:
        case X86_INS_BSF: {
            this->insnSemRemoveOP(ctx, this->OPFactory.getTypeInferOPByX86OP(operands[0]));
            break;
        }
        case X86_INS_BSWAP: {
            this->insnSemOP1NOP(insn);
            break;
        }
        case X86_INS_SHLX:
        case X86_INS_SHLD:
        case X86_INS_SARX:
        case X86_INS_SHRX:
        case X86_INS_SHRD:
        case X86_INS_RORX: {
            this->insnSemOP3NOP(insn);
            break;
        }
        case X86_INS_SAR:
        case X86_INS_SHL:
        case X86_INS_SHR:
        case X86_INS_ROL:
        case X86_INS_ROR: {
            this->insnSemOP2NOP(insn);
            break;
        }
        case X86_INS_MULSD:
        case X86_INS_MULSS:
        case X86_INS_IMUL:
        case X86_INS_MUL: {
            break;
        }
        case X86_INS_DIVSD:
        case X86_INS_DIVSS:
        case X86_INS_IDIV:
        case X86_INS_DIV: {
            break;
        }
        case X86_INS_NEG: {
            this->insnSemOP1NOP(insn);
            break;
        }
        case X86_INS_SUBSD:
        case X86_INS_SUBSS:
        case X86_INS_SBB:
        case X86_INS_SUB:
        case X86_INS_ADDSD:
        case X86_INS_ADDSS:
        case X86_INS_ADC:
        case X86_INS_ADCX:
        case X86_INS_XADD:
        case X86_INS_ADD: {
            if(operands[0].type == X86_OP_REG && operands[0].reg == X86_REG_RSP)
                break;

            this->insnSemForwardOP2ADDSUBFamily(ctx, insn, cfa);
            break;
        }
        // nop insns
        case X86_INS_UD0:
        case X86_INS_UD1:
        case X86_INS_UD2:
        case X86_INS_CPUID:
        case X86_INS_PAUSE:
        case X86_INS_PREFETCH:
        case X86_INS_PREFETCHT0:
        case X86_INS_PREFETCHT1:
        case X86_INS_PREFETCHT2:
        case X86_INS_PREFETCHW:
        case X86_INS_PREFETCHWT1:
        case X86_INS_PREFETCHNTA: break;
        default: FIG_PANIC("Unsupported insntruction! ", CFG::insnToString(insn));
        }

        if(ctx == insnOUTContext) {
            return false;
        } else {
            insnOUTContext = ctx;
            return true;
        }
    }

    void TIAnalysis::insnSemForwardOP2Src123Dst13(TIContext &ctx, [[maybe_unused]] const cs_insn &insn, const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););

        const cs_x86_op *operands = x86Detail.operands;
        if(operands[1].type == X86_OP_IMM)
            return;

        auto op0 = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        auto op1 = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        // With KnownVars, op1 => op0
        CFG::VTypePointer op0Type = nullptr;
        CFG::VTypePointer op1Type = nullptr;
        op1Type                   = this->getVTypeWithKnownVars(cfa, op1, insn, false);
        if(op1Type) {
            ctx[op1] = op1Type;
            ctx[op0] = op1Type;
            return;
        }
        // With PreContext, op1 => op0
        op1Type = this->getVTypeWithContext(op1, ctx);
        if(op1Type) {
            ctx[op1] = op1Type;
            ctx[op0] = op1Type;
            return;
        }
        // With KnownVars, op0 => op1
        op0Type = this->getVTypeWithKnownVars(cfa, op0, insn, false);
        if(op0Type) {
            ctx[op1] = op0Type;
            ctx[op0] = op0Type;
            return;
        }
        // With UnknownVars, Get op1; op1 => op0
        op1Type = this->getVTypeWithUnknownVars(cfa, op1, false);
        if(op1Type) {
            ctx[op1] = op1Type;
            ctx[op0] = op1Type;
            return;
        }
        // With UnknownVars, Get op0; op0 => op1
        op0Type = this->getVTypeWithUnknownVars(cfa, op0, false);
        if(op0Type) {
            ctx[op1] = op0Type;
            ctx[op0] = op0Type;
            return;
        }
        // Fail
        this->insnSemRemoveOP(ctx, op0);
    }

    void TIAnalysis::insnSemForwardOP2LOGICFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_IMM) {
            if(!ctx.contains(op0)) {
                switch(op0->size) {
                case 1: ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint8; break;
                case 2: ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint16; break;
                case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint32; break;
                case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint64; break;
                default: FIG_PANIC("unsupported size for logic insn! size=", op0->size);
                }
            }
        } else {
            this->insnSemForwardOP2Src123Dst13(ctx, insn, cfa);
        }
    }

    void TIAnalysis::insnSemForwardOP2CMPFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_IMM && operands[1].imm != 0) {
            if(!ctx.contains(op0)) {
                switch(op0->size) {
                case 1: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int8; break;
                case 2: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int16; break;
                case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int32; break;
                case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int64; break;
                default: FIG_PANIC("unsupported size for cmp insn! size=", op0->size);
                }
            }
        } else if(operands[0].type == X86_OP_REG && TIOperandFactory::isXMMReg(operands[0].reg) && operands[1].type == X86_OP_MEM && TIOperandFactory::isGlobalMem(operands[1].mem)) {
            if(!ctx.contains(op0)) {
                switch(op1->size) {
                case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float32; break;
                case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float64; break;
                case 16: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float128; break;
                default: FIG_PANIC("unsupported size for cmp insn! size=", op0->size);
                }
            }
            ctx[op1] = ctx[op0];
        } else {
            this->insnSemForwardOP2Src123Dst13(ctx, insn, cfa);
        }
    }

    void TIAnalysis::insnSemForwardOP2ADDSUBFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        if(operands[1].type == X86_OP_IMM)
            return;

        TIOperandRef op0 = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1 = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        auto op0lambda   = [&ctx, op0, op1](CFG::VTypePointer vType) {
            if(CFG::VTypeFactory::isIntegerType(vType)) {
                ctx[op0] = vType;
                if(!ctx.contains(op1))
                    ctx[op1] = vType;
            } else if(CFG::VTypeFactory::isPointerType(vType)) {
                ctx[op0] = vType;
                if(!ctx.contains(op1))
                    ctx[op1] = CFG::VTypeFactory::getIntegerType(true, op1->size);
            }
        };
        CFG::VTypePointer op0Type = nullptr;
        op0Type                   = this->getVTypeWithContext(op0, ctx);
        if(op0Type) {
            op0lambda(op0Type);
            return;
        }
        op0Type = this->getVTypeWithKnownVars(cfa, op0, insn, false);
        if(op0Type) {
            op0lambda(op0Type);
            return;
        }
        op0Type = this->getVTypeWithUnknownVars(cfa, op0, false);
        if(op0Type) {
            op0lambda(op0Type);
            return;
        }

        auto op1lambda = [&ctx, op0, op1](CFG::VTypePointer vType) {
            if(CFG::VTypeFactory::isIntegerType(vType)) {
                ctx[op1] = vType;
                if(!ctx.contains(op0))
                    ctx[op0] = vType;
            } else if(CFG::VTypeFactory::isPointerType(vType)) {
                ctx[op1] = vType;
                if(!ctx.contains(op0))
                    ctx[op0] = vType;
            }
        };
        CFG::VTypePointer op1Type = nullptr;
        op1Type                   = this->getVTypeWithContext(op1, ctx);
        if(op1Type) {
            op1lambda(op1Type);
            return;
        }
        op1Type = this->getVTypeWithKnownVars(cfa, op1, insn, false);
        if(op1Type) {
            op1lambda(op1Type);
            return;
        }
        op1Type = this->getVTypeWithUnknownVars(cfa, op1, false);
        if(op1Type) {
            op1lambda(op1Type);
            return;
        }
    }

    void TIAnalysis::insnSemForwardOP2MOVFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        if(operands[1].type == X86_OP_MEM && operands[1].mem.segment == X86_REG_FS && operands[1].mem.base == X86_REG_INVALID && operands[1].mem.index == X86_REG_INVALID && operands[1].mem.disp == 0x28) {
            // mov rax, fs:[0x28]; The cannary value
            ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint64;
            ctx[op1] = ctx[op0];
        } else if((operands[0].type == X86_OP_REG || operands[0].type == X86_OP_MEM) && operands[1].type == X86_OP_IMM) {
            switch(operands[0].size) {
            case 1: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int8; break;
            case 2: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int16; break;
            case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int32; break;
            case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int64; break;
            default: FIG_PANIC("unreached path!");
            }
        } else if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_MEM && operands[1].mem.base == X86_REG_RIP) {
            if(TIOperandFactory::isXMMReg(operands[0].reg)) {
                switch(operands[1].size) {
                case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float32; break;
                case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float64; break;
                case 16: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float128; break;
                default: FIG_PANIC("unreached path! operands[0].size=", operands[0].size);
                }
            } else {
                switch(operands[0].size) {
                case 1: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int8; break;
                case 2: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int16; break;
                case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int32; break;
                case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int64; break;
                default: FIG_PANIC("unreached path!");
                }
            }
            ctx[op1] = ctx[op0];
        } else { // default
            this->insnSemForwardOP2Src123Dst13(ctx, insn, cfa);
        }
    }

    void TIAnalysis::insnSemForwardOP2MOVZX(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);

        ctx[op0] = CFG::VTypeFactory::getIntegerType(false, op0->size);
        if(!ctx.contains(op1))
            ctx[op1] = CFG::VTypeFactory::getIntegerType(false, op1->size);
    }

    void TIAnalysis::insnSemForwardOP2MOVSX(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        ctx[op0]                  = CFG::VTypeFactory::getIntegerType(true, op0->size);
        if(!ctx.contains(op1))
            ctx[op1] = CFG::VTypeFactory::getIntegerType(true, op1->size);
    }

    void TIAnalysis::insnSemForwardOP2MOVSXD(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        ctx[op0]                  = CFG::VTypeFactory::baseVType.bt_int64;
        if(!ctx.contains(op1))
            ctx[op1] = CFG::VTypeFactory::baseVType.bt_int32;
    }

    void TIAnalysis::insnSemForwardOP1Call(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 1, "op_count != 1");
        this->clearCalledOP(ctx);
        if(!this->config.enableInterProceduralAnalysis)
            return;

        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef callTargetOP = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        if(callTargetOP->type != X86_OP_IMM)
            return;
        int64_t callTargetAddr    = callTargetOP->detail.imm;
        CFG::VTypePointer retType = static_cast<CFG::VTypePointer>(nullptr);
        if(this->customFuncMap->contains(callTargetAddr)) {
            retType = (*this->customFuncMap.get())[callTargetAddr]->retVType;
        } else if(this->config.enableDynLibFuncAnalysis && this->dynLibFuncMap->contains(callTargetAddr)) {
            // FIG_PANIC("HIT DYNAMIC FUNTION!", "  ADDR: ", CFG::dec_to_hex(callTargetAddr), "  FUNC:", this->dynLibFuncMap->at(callTargetAddr)->name);
            retType = (*this->dynLibFuncMap.get())[callTargetAddr]->retVType;
        } else {
            // do nothing for indirect call or unknown function
            return;
        }

        if(retType->typeTag == CFG::TYPETAG::BASE) {
            const auto baseType = retType->getBaseType();
            switch(baseType) {
            case CFG::BASETYPE::BT_BOOL: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_AL), CFG::VTypeFactory::baseVType.bt_bool}); break;
            case CFG::BASETYPE::BT_UINT8: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_AL), CFG::VTypeFactory::baseVType.bt_uint8}); break;
            case CFG::BASETYPE::BT_UINT16: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_AX), CFG::VTypeFactory::baseVType.bt_uint16}); break;
            case CFG::BASETYPE::BT_UINT32: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_EAX), CFG::VTypeFactory::baseVType.bt_uint32}); break;
            case CFG::BASETYPE::BT_UINT64: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RAX), CFG::VTypeFactory::baseVType.bt_uint64}); break;
            case CFG::BASETYPE::BT_UINT128: {
                ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RAX), CFG::VTypeFactory::baseVType.bt_uint64});
                ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RDX), CFG::VTypeFactory::baseVType.bt_uint64});
                break;
            }
            case CFG::BASETYPE::BT_INT8: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_AL), CFG::VTypeFactory::baseVType.bt_int8}); break;
            case CFG::BASETYPE::BT_INT16: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_AX), CFG::VTypeFactory::baseVType.bt_int16}); break;
            case CFG::BASETYPE::BT_INT32: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_EAX), CFG::VTypeFactory::baseVType.bt_int32}); break;
            case CFG::BASETYPE::BT_INT64: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RAX), CFG::VTypeFactory::baseVType.bt_int64}); break;
            case CFG::BASETYPE::BT_INT128: {
                ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RAX), CFG::VTypeFactory::baseVType.bt_int64});
                ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RDX), CFG::VTypeFactory::baseVType.bt_int64});
                break;
            }
            case CFG::BASETYPE::BT_FLOAT32: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_XMM0), CFG::VTypeFactory::baseVType.bt_float32}); break;
            case CFG::BASETYPE::BT_FLOAT64: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_XMM0), CFG::VTypeFactory::baseVType.bt_float64}); break;
            case CFG::BASETYPE::BT_FLOAT128: ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_XMM0), CFG::VTypeFactory::baseVType.bt_float128}); break;
            default: FIG_PANIC("unreached path! ", "baseType:", static_cast<uint32_t>(baseType));
            }
        } else if(retType->typeTag == CFG::TYPETAG::POINTER) {
            ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RAX), retType});
        } else if(retType->typeTag == CFG::TYPETAG::STRUCTURE || retType->typeTag == CFG::TYPETAG::UNION) {
            if(retType->size <= 128) {
                // do nothing
            } else {
                // pass address by rax
                ctx.insert({this->OPFactory.getRegOPByx86Reg(X86_REG_RAX), this->typeCalculator.getPointerVType(retType)});
            }
        }
    }

    void TIAnalysis::insnSemForwardOP2LEA(TIContext &ctx, const cs_insn &insn, const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count != 2"););
        const cs_x86_op *operands = x86Detail.operands;
        auto op0                  = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        auto op1                  = this->OPFactory.getTypeInferOPByX86OP(operands[1]);

        /// special case: lea rax, [rax+1];
        if(op0->type == X86_OP_REG && op1->type == X86_OP_MEM && op1->detail.mem.index == X86_REG_INVALID && op1->detail.mem.segment == X86_REG_INVALID) {
            auto opBaseReg = this->OPFactory.getRegOPByx86Reg(op1->detail.mem.base);
            if(ctx.contains(opBaseReg)) {
                auto opBaseRegVType = ctx[opBaseReg];
                if(CFG::VTypeFactory::isIntegerType(opBaseRegVType)) {
                    if(CFG::VTypeFactory::isSignedIntType(opBaseRegVType))
                        ctx[op0] = CFG::VTypeFactory::getIntegerType(true, op0->size);
                    else
                        ctx[op0] = CFG::VTypeFactory::getIntegerType(false, op0->size);
                    ctx[op1] = CFG::VTypeFactory::baseVType.bt_int64;
                    return;
                }
            }
        }

        // op1 => op0
        auto op1Type = this->getVTypeWithKnownVars(cfa, op1, insn, true);
        if(op1Type) {
            ctx[op0]        = op1Type;
            auto objectType = this->typeCalculator.getObjectVType(op1Type);
            if(objectType != CFG::VTypeFactory::baseVType.bt_void) {
                auto firstVType = objectType->getFirstVTypeInMemoryModel(op1->size, CFG::VTypeFactory::baseVType.bt_void);
                if(firstVType != CFG::VTypeFactory::baseVType.bt_void)
                    ctx[op1] = firstVType;
            }
            return;
        }
        // op0 => op1
        auto op0Type = this->getVTypeWithKnownVars(cfa, op0, insn, false);
        if(op0Type) {
            ctx[op0] = op0Type;
            if(op0Type->typeTag != CFG::TYPETAG::POINTER)
                return;
            auto objectType = this->typeCalculator.getObjectVType(op0Type);
            if(objectType != CFG::VTypeFactory::baseVType.bt_void) {
                auto firstVType = objectType->getFirstVTypeInMemoryModel(op1->size, CFG::VTypeFactory::baseVType.bt_void);
                if(firstVType != CFG::VTypeFactory::baseVType.bt_void)
                    ctx[op1] = firstVType;
            }
            return;
        }
        // With Precontext
        op1Type = this->getVTypeWithContext(op1, ctx);
        if(op1Type) {
            ctx[op1] = op1Type;
            ctx[op0] = this->typeCalculator.getPointerVType(op1Type);
            return;
        }

        // With UnknownVars
        // Get op1; op1 => op0
        op1Type = this->getVTypeWithUnknownVars(cfa, op1, true);
        if(op1Type) {
            ctx[op0]        = op1Type;
            auto objectType = this->typeCalculator.getObjectVType(op1Type);
            if(objectType != CFG::VTypeFactory::baseVType.bt_void) {
                auto firstVType = objectType->getFirstVTypeInMemoryModel(op1->size, CFG::VTypeFactory::baseVType.bt_void);
                if(firstVType != CFG::VTypeFactory::baseVType.bt_void)
                    ctx[op1] = firstVType;
            }
            return;
        }
        // Get op0; op0 => op1
        op0Type = this->getVTypeWithUnknownVars(cfa, op0, false);
        if(op0Type && op0Type->typeTag == CFG::TYPETAG::POINTER) {
            auto objectType = this->typeCalculator.getObjectVType(op0Type);
            if(objectType != CFG::VTypeFactory::baseVType.bt_void) {
                auto firstVType = objectType->getFirstVTypeInMemoryModel(op1->size, CFG::VTypeFactory::baseVType.bt_void);
                if(firstVType != CFG::VTypeFactory::baseVType.bt_void)
                    ctx[op1] = firstVType;
                ctx[op0] = op0Type;
            }
            return;
        }
        // Fail
        this->insnSemRemoveOP(ctx, op0);
    }
} // namespace SA