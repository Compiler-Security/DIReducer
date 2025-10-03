#include <cstdint>             // for uint32_t, int32_t, int64_t
#include <memory>              // for shared_ptr, __shared_ptr_access, allo...
#include <optional>            // for optional
#include <set>                 // for set
#include <unordered_map>       // for unordered_map, operator==
#include <vector>              // for vector
#include "CFG/AsmBlock.h"      // for AsmBlock, BLOCK_TYPE
#include "CFG/AsmCFG.h"        // for AsmCFG
#include "CFG/AsmStatement.h"  // for insnToString
#include "CFG/Func.h"          // for Func
#include "CFG/Type.h"          // for VTypePointer, TYPETAG
#include "CFG/TypeFactory.h"   // for VTypeFactory
#include "CFG/Var.h"           // for Var
#include "Fig/assert.hpp"      // for FIG_ASSERT
#include "Fig/panic.hpp"       // for FIG_PANIC
#include "Core/Analysis.h"     // for TIAnalysis, CFA
#include "Core/TIConfigure.h"    // for TIConfigure
#include "Core/TIContext.h"      // for TIContext
#include "Core/TIOperand.h"      // for TIOperandFactory, TIOperandRef
#include "Core/TypeCalculator.h" // for TypeCalculator
#include "capstone/capstone.h" // for cs_insn
#include "capstone/x86.h"      // for x86_insn, x86_reg, x86_op_type, cs_x8...

namespace SA {
    void TIAnalysis::backwardInference() {
        auto exitBlockOptional = this->func->cfg.value().getExitBlock();
        if(!exitBlockOptional.has_value())
            return;

        auto exitBlock = exitBlockOptional.value();
        std::set<uint32_t> snVisited{};
        this->backwardIterativeExecution(snVisited, exitBlock);
    }

    void TIAnalysis::backwardIterativeExecution(std::set<uint32_t> &snVisited, std::shared_ptr<CFG::AsmBlock> &block) {
        if(snVisited.contains(block->sn))
            return;
        snVisited.insert(block->sn);

        this->backwardInferenceInBlock(block);
        for(auto &it : block->getPredsExceptCall()) {
            this->backwardIterativeExecution(snVisited, it);
        }
    }

    bool TIAnalysis::backwardInferenceInBlock(const std::shared_ptr<CFG::AsmBlock> &block) {
        if(block->type == CFG::BLOCK_TYPE::ENTRY_BLOCK || block->type == CFG::BLOCK_TYPE::EXIT_BLOCK) {
            this->inBlockContexts[block->sn] = this->outBlockContexts[block->sn];
            return false;
        }

        auto &outContext = this->outBlockContexts[block->sn];
        TIContext baseContext;
        for(auto &fromBlock : block->getSuccsExceptCall()) {
            const auto &fromContext = this->inBlockContexts[fromBlock->sn];
            this->unionTIContext(baseContext, fromContext);
        }
        outContext                 = baseContext;
        TIContext &lastInsnContext = this->blockInsnContexts[block->sn].back();
        lastInsnContext            = baseContext;

        TIContext tempInContext;
        for(int32_t i = block->statements.size() - 1; i >= 0; i--) {
            auto &insn       = block->statements[i];
            auto &insnCFA    = this->blockInsnCFA[block->sn][i];
            auto &preContext = this->blockInsnContexts[block->sn][i];
            if(i == 0) {
                this->backwardInferenceInStatement(preContext, tempInContext, insnCFA, insn);
            } else {
                this->backwardInferenceInStatement(preContext, this->blockInsnContexts[block->sn][i - 1], insnCFA, insn);
            }
        }

        auto &inContext = this->inBlockContexts[block->sn];
        if(tempInContext == inContext) {
            return false;
        } else {
            inContext = tempInContext;
            return true;
        }
    }

    bool TIAnalysis::backwardInferenceInStatement(const TIContext &insnOUTContext, TIContext &insnINContext, const CFA &cfa, const cs_insn &insn) {
        TIContext ctx = insnOUTContext;
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
        case X86_INS_CALL: this->insnSemBackwardOP1Call(ctx, insn); break;
        case X86_INS_MOVZX: this->insnSemBackwardOP2MOVZX(ctx, insn); break;
        case X86_INS_MOVSX: this->insnSemBackwardOP2MOVSX(ctx, insn); break;
        case X86_INS_MOVSXD: this->insnSemBackwardOP2MOVSXD(ctx, insn); break;
        case X86_INS_MOVDQA:
        case X86_INS_MOVABS:
        case X86_INS_MOVAPS:
        case X86_INS_MOVAPD:
        case X86_INS_MOVSS:
        case X86_INS_MOVSD:
        case X86_INS_MOVSQ:
        case X86_INS_MOVQ:
        case X86_INS_MOVD:
        case X86_INS_MOV: this->insnSemBackwardOP2MOVFamily(ctx, insn, cfa); break;
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
        case X86_INS_CMOVBE: this->insnSemBackwardOP2MOVFamily(ctx, insn, cfa); break;
        case X86_INS_LEA: this->insnSemBackwardOP2LEA(ctx, insn, cfa); break;
        case X86_INS_NOP:
        case X86_INS_RET:
        case X86_INS_LEAVE:
        case X86_INS_ENDBR64: this->insnSemOP0NOP(insn); break;
        // push && pop
        case X86_INS_PUSH:
        case X86_INS_POP: this->insnSemOP1NOP(insn); break;
        // rax => rdx:rax
        case X86_INS_CQO: {
            this->insnSemRemoveReg(ctx, X86_REG_DL);
            this->insnSemRemoveReg(ctx, X86_REG_DX);
            this->insnSemRemoveReg(ctx, X86_REG_EDX);
            this->insnSemRemoveReg(ctx, X86_REG_RDX);
            break;
        }
        // eax => edx:eax
        case X86_INS_CDQ: {
            this->insnSemRemoveReg(ctx, X86_REG_DL);
            this->insnSemRemoveReg(ctx, X86_REG_DX);
            this->insnSemRemoveReg(ctx, X86_REG_EDX);
            this->insnSemRemoveReg(ctx, X86_REG_RDX);
            break;
        }
        case X86_INS_CBW: {
            this->insnSemRemoveReg(ctx, X86_REG_AL);
            this->insnSemRemoveReg(ctx, X86_REG_AX);
            this->insnSemRemoveReg(ctx, X86_REG_EAX);
            this->insnSemRemoveReg(ctx, X86_REG_RAX);
            break;
        }
        case X86_INS_CWD: {
            this->insnSemRemoveReg(ctx, X86_REG_DL);
            this->insnSemRemoveReg(ctx, X86_REG_DX);
            this->insnSemRemoveReg(ctx, X86_REG_EDX);
            this->insnSemRemoveReg(ctx, X86_REG_RDX);
            break;
        }
        // al => ax
        case X86_INS_CWDE: {
            this->insnSemRemoveReg(ctx, X86_REG_AL);
            this->insnSemRemoveReg(ctx, X86_REG_AX);
            this->insnSemRemoveReg(ctx, X86_REG_EAX);
            this->insnSemRemoveReg(ctx, X86_REG_RAX);
            cs_x86_op csX86OP;
            csX86OP.type     = X86_OP_REG;
            csX86OP.reg      = X86_REG_AL;
            csX86OP.size     = 1;
            auto typeInferOP = this->OPFactory.getTypeInferOPByX86OP(csX86OP);
            ctx.insert({typeInferOP, CFG::VTypeFactory::baseVType.bt_int8});
            break;
        }
        // eax => rax
        case X86_INS_CDQE: {
            this->insnSemRemoveReg(ctx, X86_REG_AL);
            this->insnSemRemoveReg(ctx, X86_REG_AX);
            this->insnSemRemoveReg(ctx, X86_REG_EAX);
            this->insnSemRemoveReg(ctx, X86_REG_RAX);
            cs_x86_op csX86OP;
            csX86OP.type     = X86_OP_REG;
            csX86OP.reg      = X86_REG_EAX;
            csX86OP.size     = 4;
            auto typeInferOP = this->OPFactory.getTypeInferOPByX86OP(csX86OP);
            ctx.insert({typeInferOP, CFG::VTypeFactory::baseVType.bt_int32});
            break;
        }
        case X86_INS_STOSQ: {
            this->insnSemBackwardOP2MOVFamily(ctx, insn, cfa);
            break;
        }
        case X86_INS_MOVMSKPS:
        case X86_INS_MOVMSKPD: {
            FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");
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
            FIG_ASSERT(x86Detail.op_count == 1, "op_count != 1");
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
            FIG_ASSERT(x86Detail.op_count == 1, "op_count != 1");
            this->insnSemRemoveOP(ctx, this->OPFactory.getTypeInferOPByX86OP(operands[0]));
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
            FIG_ASSERT(x86Detail.op_count == 1, "op_count != 1");
            this->insnSemRemoveOP(ctx, this->OPFactory.getTypeInferOPByX86OP(operands[0]));
            break;
        }
        case X86_INS_FLDCW: {
            this->insnSemOP1NOP(insn);
            break;
        }
        // float => int, flost stack -> mem
        case X86_INS_FISTTP:
        case X86_INS_FISTP: {
            FIG_ASSERT(x86Detail.op_count == 1, "op_count != 1");
            this->insnSemRemoveOP(ctx, this->OPFactory.getTypeInferOPByX86OP(operands[0]));
            break;
        }
        // int => float, mem -> flost stack
        case X86_INS_FILD: {
            FIG_ASSERT(x86Detail.op_count == 1, "op_count != 1");
            this->insnSemRemoveOP(ctx, this->OPFactory.getTypeInferOPByX86OP(operands[0]));
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
        case X86_INS_CMP: this->insnSemBackwardOP2CMPFamily(ctx, insn, cfa); break;
        // test
        case X86_INS_TEST:
        // and
        case X86_INS_AND:
        case X86_INS_PAND:
        case X86_INS_ANDPD:
        case X86_INS_ANDPS:
        // andn
        case X86_INS_ANDN:
        case X86_INS_PANDN:
        case X86_INS_ANDNPD:
        case X86_INS_ANDNPS:
        // xor
        case X86_INS_XOR:
        case X86_INS_PXOR:
        case X86_INS_XORPD:
        case X86_INS_XORPS:
        // or
        case X86_INS_OR:
        case X86_INS_POR:
        case X86_INS_ORPD:
        case X86_INS_ORPS: {
            this->insnSemBackwardOP2LOGICFamily(ctx, insn, cfa);
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
            if(operands[0].type != X86_OP_REG) FIG_PANIC("op is not reg!");
            this->insnSemRemoveReg(ctx, operands[0].reg);
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
            /* add/sub rsp, imm */
            if(operands[0].type == X86_OP_REG && operands[0].reg == X86_REG_RSP)
                break;

            this->insnSemBackwardOP2ADDSUBFamily(ctx, insn, cfa);
            break;
        }

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

        if(ctx == insnINContext) {
            return false;
        } else {
            insnINContext = ctx;
            return true;
        }
    }

    void TIAnalysis::insnSemBackwardOP2ADDSUBFamily(TIContext &ctx, const cs_insn &insn, [[maybe_unused]] const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");

        const cs_x86_op *operands = x86Detail.operands;
        if(operands[1].type == X86_OP_IMM)
            return;

        TIOperandRef op0 = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1 = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        auto op0Type     = this->getVTypeWithContext(op0, ctx);
        if(op0Type) {
            if(CFG::VTypeFactory::isIntegerType(op0Type)) {
                ctx[op0] = op0Type;
                if(!ctx.contains(op1))
                    ctx[op1] = op0Type;
            } else if(CFG::VTypeFactory::isPointerType(op0Type)) {
                ctx[op0] = op0Type;
                if(!ctx.contains(op1))
                    ctx[op1] = CFG::VTypeFactory::getIntegerType(true, op1->size);
            }
            return;
        }

        auto op1Type = this->getVTypeWithContext(op1, ctx);
        if(op1Type) {
            if(CFG::VTypeFactory::isIntegerType(op1Type)) {
                ctx[op1] = op1Type;
                if(!ctx.contains(op0))
                    ctx[op0] = op1Type;
            } else if(CFG::VTypeFactory::isPointerType(op1Type)) {
                ctx[op1] = op1Type;
                if(!ctx.contains(op0))
                    ctx[op0] = op1Type;
            }
            return;
        }
    }

    void TIAnalysis::insnSemBackwardOP2LOGICFamily(TIContext &ctx, const cs_insn &insn, [[maybe_unused]] const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");

        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_IMM) { // 与立即数做运算,可以反推整数类型
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
            this->insnSemBackwardOP2EQ(ctx, insn);
        }
    }

    void TIAnalysis::insnSemBackwardOP2CMPFamily(TIContext &ctx, const cs_insn &insn, [[maybe_unused]] const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");

        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_IMM && operands[1].imm != 0) { // 与立即数比较,可以反推整数类型
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
            this->insnSemBackwardOP2EQ(ctx, insn);
        }
    }

    void TIAnalysis::insnSemBackwardOP2EQ(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");

        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        CFG::VTypePointer op0Type = this->getVTypeWithContext(op0, ctx);
        CFG::VTypePointer op1Type = this->getVTypeWithContext(op1, ctx);
        if(op0Type && op1Type) {
            auto unionType = this->typeCalculator.getUnionVType(op0Type, op1Type);
            ctx[op0]       = unionType;
            ctx[op1]       = unionType;
            return;
        } else if(op0Type) {
            ctx[op0] = op0Type;
            ctx[op1] = op0Type;
            return;
        } else if(op1Type) {
            ctx[op0] = op1Type;
            ctx[op1] = op1Type;
            return;
        }
    }

    void TIAnalysis::insnSemBackwardOP2Dst1(TIContext &ctx, const cs_insn &insn, [[maybe_unused]] const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");

        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        CFG::VTypePointer op0Type = this->getVTypeWithContext(op0, ctx);
        if(op0Type) {
            ctx[op1] = op0Type;
            this->insnSemRemoveOP(ctx, op0);
            return;
        }

        // Fail to infer type!
        this->insnSemRemoveOP(ctx, op0);
    }

    void TIAnalysis::insnSemBackwardOP2LEA(TIContext &ctx, const cs_insn &insn, [[maybe_unused]] const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op0          = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        CFG::VTypePointer op0Type = this->getVTypeWithContext(op0, ctx);
        if(op0Type) {
            if(op0Type->typeTag != CFG::TYPETAG::POINTER)
                return;
            auto objectType = this->typeCalculator.getObjectVType(op0Type);
            if(objectType != CFG::VTypeFactory::baseVType.bt_void) {
                auto firstVType = objectType->getFirstVTypeInMemoryModel(op1->size, CFG::VTypeFactory::baseVType.bt_void);
                if(firstVType != CFG::VTypeFactory::baseVType.bt_void)
                    ctx[op1] = firstVType;
            }
            this->insnSemRemoveOP(ctx, op0);
        }

        // Fail to infer type!
        this->insnSemRemoveOP(ctx, op0);
    }

    void TIAnalysis::insnSemBackwardOP2MOVFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");
        const cs_x86_op *operands = x86Detail.operands;
        if(operands[1].type == X86_OP_MEM && operands[1].mem.segment == X86_REG_FS && operands[1].mem.base == X86_REG_INVALID && operands[1].mem.index == X86_REG_INVALID && operands[1].mem.disp == 0x28) {
            // mov rax, fs:[0x28]; The cannary value
            return;
        } else if((operands[0].type == X86_OP_REG || operands[0].type == X86_OP_MEM) && operands[1].type == X86_OP_IMM) {
            return;
        } else if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_MEM && operands[1].mem.base == X86_REG_RIP) {
            return;
        } else {
            this->insnSemBackwardOP2Dst1(ctx, insn, cfa);
        }
    }

    void TIAnalysis::insnSemBackwardOP1Call(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 1, "op_count != 1");
        this->clearCalledOP(ctx);
        if(!this->config.enableInterProceduralAnalysis)
            return;
        if(!this->config.enableDynLibFuncAnalysis)
            return;
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef callTargetOP = this->OPFactory.getTypeInferOPByX86OP(operands[0]);
        if(callTargetOP->type != X86_OP_IMM)
            return;
        int64_t callTargetAddr = callTargetOP->detail.imm;
        if(!this->dynLibFuncMap->contains(callTargetAddr))
            return;
        std::shared_ptr<CFG::Func> dynFunc = (*this->dynLibFuncMap.get())[callTargetAddr];
        auto isSystemVABIIntegerClass      = [](CFG::VTypePointer vType) -> bool {
            return vType->size <= 8 && (CFG::VTypeFactory::isPointerType(vType) || CFG::VTypeFactory::isBoolType(vType) || CFG::VTypeFactory::isIntegerType(vType));
        };
        auto isSystemVABISSEClass = [](CFG::VTypePointer vType) -> bool {
            return vType->size <= 8 && (CFG::VTypeFactory::isFloatType(vType));
        };
        // reference System-V-ABI-AMD64.pdf, link: https://gitlab.com/x86-psABIs/x86-psABI/
        static x86_reg IntegerClassRegisters[6][2] = {
            {X86_REG_EDI, X86_REG_RDI},
            {X86_REG_ESI, X86_REG_RSI},
            {X86_REG_EDX, X86_REG_RDX},
            {X86_REG_ECX, X86_REG_RCX},
            {X86_REG_R8D, X86_REG_R8},
            {X86_REG_R9D, X86_REG_R9}};
        static x86_reg SSEClassRegisters[8] = {
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
        for(const auto &v : dynFunc->variableList) {
            auto vType = v.vType;
            if(isSystemVABIIntegerClass(vType)) {
                uint32_t bitwidthFlag    = vType->size > 4 ? 1 : 0;
                x86_reg targetReg        = IntegerClassRegisters[IntegerClassIndex][bitwidthFlag];
                TIOperandRef targetRegOP = this->OPFactory.getRegOPByx86Reg(targetReg);
                ctx[targetRegOP]         = vType;
                //  next Integer Class register
                IntegerClassIndex++;
            } else if(isSystemVABISSEClass(vType)) {
                x86_reg targetReg        = SSEClassRegisters[SSEClassIndex];
                TIOperandRef targetRegOP = this->OPFactory.getRegOPByx86Reg(targetReg);
                ctx[targetRegOP]         = vType;
                //  next SSE Class register
                SSEClassIndex++;
            } else {
                FIG_PANIC("Unsupported parameter type for Dynamic Function! ", " TypeName: ", vType->name);
            }
        }
    }

    void TIAnalysis::insnSemBackwardOP2MOVZX(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        ctx[op1]                  = CFG::VTypeFactory::getIntegerType(false, op1->size);
    }

    void TIAnalysis::insnSemBackwardOP2MOVSX(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        ctx[op1]                  = CFG::VTypeFactory::getIntegerType(true, op1->size);
    }

    void TIAnalysis::insnSemBackwardOP2MOVSXD(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_ASSERT(x86Detail.op_count == 2, "op_count != 2");
        const cs_x86_op *operands = x86Detail.operands;
        TIOperandRef op1          = this->OPFactory.getTypeInferOPByX86OP(operands[1]);
        ctx[op1]                  = CFG::VTypeFactory::baseVType.bt_int32;
    }
} // namespace SA