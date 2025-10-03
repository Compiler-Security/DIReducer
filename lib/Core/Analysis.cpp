#include <capstone/capstone.h>   // for cs_insn
#include <capstone/x86.h>        // for x86_reg, x86_insn, x86_op_type
#include <chrono>                // for duration, duration_cast, ope...
#include <cstdint>               // for uint32_t, uint16_t, uint64_t
#include <filesystem>            // for path, operator/, create_dire...
#include <fstream>               // for basic_ofstream, basic_ostream
#include <iterator>              // for next
#include <memory>                // for allocator, shared_ptr, __sha...
#include <optional>              // for optional
#include <ranges>                // for filter_view, operator|, _Filter
#include <set>                   // for operator==, set
#include <string>                // for basic_string, char_traits
#include <unordered_map>         // for unordered_map, operator==
#include <unordered_set>         // for unordered_set
#include <utility>               // for pair
#include <vector>                // for vector
#include "CFG/AsmBlock.h"        // for AsmBlock
#include "CFG/AsmCFG.h"          // for AsmCFG
#include "CFG/AsmStatement.h"    // for insnToString
#include "CFG/Func.h"            // for Func
#include "CFG/Type.h"            // for VType, VTypePointer, TYPETAG
#include "CFG/TypeFactory.h"     // for VTypeFactory
#include "CFG/Utils.h"           // for dec_to_hex, x86_op_mem_to_st...
#include "CFG/Var.h"             // for Var, VarAddr, ADDR_TYPE
#include "Fig/debug.hpp"         // for FIG_DEBUG
#include "Fig/panic.hpp"         // for FIG_PANIC
#include "Core/Analysis.h"       // for TIAnalysis, CFA
#include "Core/TIConfigure.h"      // for TIConfigure
#include "Core/TIContext.h"        // for TIContext
#include "Core/TIOperand.h"        // for TIOperandFactory, TIOperand
#include "Core/RuntimeInfo.h"    // for TIRuntimeInfo
#include "Core/TypeCalculator.h"   // for TypeCalculator
#include "nlohmann/json.hpp"     // for basic_json
#include "nlohmann/json_fwd.hpp" // for json

namespace fs = std::filesystem;

namespace SA {
    TIAnalysis::TIAnalysis(
        const std::string &programName,
        const std::shared_ptr<CFG::Func> &func) {
        if(!func->hasCFG())
            FIG_PANIC("Function ", func->demangledName, " has not been initialized with CFG!");

        this->programName = programName;
        this->funcName    = func->demangledName;
        this->func        = func;
        auto &cfg         = this->func->cfg.value();
        for(auto &[_, block] : cfg.blocks) {
            const auto blockSn       = block->sn;
            const auto &statements   = block->statements;
            const auto statementSize = statements.size();
            this->blockInsnCFA.insert({blockSn, std::vector<CFA>(statementSize)});
            this->blockInsnContexts.insert({blockSn, std::vector<TIContext>(statementSize)});
            this->inBlockContexts.insert({blockSn, TIContext()});
            this->outBlockContexts.insert({blockSn, TIContext()});
        }

        this->config.enableDynLibFuncAnalysis      = true;
        this->config.enableGlobalVariablAnalysis   = true;
        this->config.enableMemOPCalculatedAnalysis = true;
        this->config.enableInterProceduralAnalysis = true;
        this->config.enableSavingRegCompensation   = true;
        this->config.enableUsingUnknownVar         = true;
    }

    TIAnalysis::~TIAnalysis() {
        os.close();
    }

    void TIAnalysis::collectResult() {
        this->collectResultIterate();
    }

    void TIAnalysis::savingResultOnFile() {
        fs::path jsonPath = fs::current_path() / "result" / "func-analysis" / this->programName / (this->func->dwarfName + ".json");
        if(!fs::exists(jsonPath.parent_path()))
            fs::create_directories(jsonPath.parent_path());
        this->os.open(jsonPath);
        if(!this->os.is_open())
            FIG_PANIC("Can not open file: ", jsonPath);

        nlohmann::json njson;
        njson["FinalContext"]   = this->toJson(false, true, true, false);
        njson["AllRuntimeInfo"] = {
            {"numObBasicBlock", this->runtimeInfo.numOfBasicBlock},
            {"numOfEdges", this->runtimeInfo.numOfEdges},
            {"numOfInsn", this->runtimeInfo.numOfInsn},
            {"timeCost(microseconds)", this->runtimeInfo.timeCost},
            {"totalOperands", this->runtimeInfo.totalOP},
            {"inferredOperands", this->runtimeInfo.inferredOP},
            {"irrelevantOperands", this->runtimeInfo.irrelevantOP},
            {"totalVarNum", this->runtimeInfo.totalVarNum},
            {"usedVarNum", this->runtimeInfo.usedVarNum}};
        this->os << njson.dump(2) << std::endl;
        this->os.close();
    }

    std::unordered_set<uint64_t> TIAnalysis::getRedundantVarIdSet() const {
        auto unusedVarFilter        = std::views::filter([this](const CFG::Var &v) {
            return (!this->knownVarsId.contains(v.varId) || v.isInfered);
        });
        auto Var2IdMapper           = std::views::transform([](const CFG::Var &v) { return v.varId; });
        auto unusedVariableIdViewer = this->func->variableList
                                      | unusedVarFilter
                                      | Var2IdMapper;

        return {unusedVariableIdViewer.begin(), unusedVariableIdViewer.end()};
    }

    void TIAnalysis::setFuncTypeMap(const std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> &funcTypeMap) {
        this->customFuncMap = funcTypeMap;
    }

    void TIAnalysis::setDynLibFuncMap(const std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> &dynLibFuncMap) {
        this->dynLibFuncMap = dynLibFuncMap;
    }

    void TIAnalysis::setGlobalVars(const std::shared_ptr<std::vector<std::shared_ptr<CFG::Var>>> &globalVars) {
        this->globalVars = globalVars;
    }

    void TIAnalysis::run() {
        this->initializeCFA();

        // begin - info record
        this->runtimeInfo.numOfBasicBlock = this->func->cfg->blocks.size();
        this->runtimeInfo.numOfEdges      = this->func->cfg->edges.size();
        this->runtimeInfo.numOfInsn       = this->func->numOfInsn;
        this->runtimeInfo.startTime       = std::chrono::steady_clock::now();

        // forward analysis
        this->config.enableUsingUnknownVar = false;
        this->forwardInference();
        // backward analysis
        this->config.enableUsingUnknownVar = false;
        this->backwardInference();
        // forward analysis
        this->config.enableUsingUnknownVar = true;
        this->forwardInference();

        // end - info record
        this->runtimeInfo.endTime = std::chrono::steady_clock::now();
    }

    void TIAnalysis::collectResultIterate() {
        auto duration                 = std::chrono::duration_cast<std::chrono::microseconds>(this->runtimeInfo.endTime - this->runtimeInfo.startTime);
        this->runtimeInfo.timeCost    = static_cast<double>(duration.count());
        this->runtimeInfo.totalOP     = 0;
        this->runtimeInfo.inferredOP  = 0;
        this->runtimeInfo.totalVarNum = this->func->variableList.size() + this->func->parameterList.size();
        this->runtimeInfo.usedVarNum  = this->knownVars.size();
        for(const auto &p : this->func->parameterList) {
            if(p.isInfered && p.isUsed) {
                this->runtimeInfo.usedVarNum--;
            }
        }
        auto entryBlock = this->func->cfg->getEntryBlock();
        std::set<uint32_t> snVisited{};
        this->collectResultRecursion(snVisited, entryBlock);
    }

    void TIAnalysis::collectResultRecursion(std::set<uint32_t> &snVisited, std::shared_ptr<CFG::AsmBlock> &block) {
        if(snVisited.contains(block->sn))
            return;
        snVisited.insert(block->sn);

        for(uint32_t i = 0; i < block->statements.size(); i++) {
            auto &insn = block->statements[i];
            FIG_DEBUG(
                if(!this->blockInsnContexts.contains(block->sn))
                    FIG_PANIC("stop, blockSn:", block->sn););

            const auto &x86Detail = insn.detail->x86;
            if(this->isIrrelevantInsn(insn)) {
                this->runtimeInfo.totalOP      = this->runtimeInfo.totalOP + x86Detail.op_count;
                this->runtimeInfo.inferredOP   = this->runtimeInfo.inferredOP + x86Detail.op_count;
                this->runtimeInfo.irrelevantOP = this->runtimeInfo.irrelevantOP + x86Detail.op_count;
                if(this->config.enableSavingRegCompensation && this->isSavingRegInsn(insn)) {
                    this->runtimeInfo.inferredOP   = this->runtimeInfo.inferredOP + 2;
                    this->runtimeInfo.irrelevantOP = this->runtimeInfo.irrelevantOP + 2;
                }
            } else {
                for(uint32_t OPIndex = 0; OPIndex < x86Detail.op_count; OPIndex++) {
                    auto &csX86OP = x86Detail.operands[OPIndex];
                    if(csX86OP.type == X86_OP_IMM) {
                        this->runtimeInfo.inferredOP++;
                    } else {
                        auto typeInferOP = this->OPFactory.getTypeInferOPByX86OP(csX86OP);
                        if(this->blockInsnContexts[block->sn][i].contains(typeInferOP))
                            this->runtimeInfo.inferredOP++;
                    }
                    this->runtimeInfo.totalOP++;
                }
            }
        }

        for(auto &it : block->getSuccsExceptCall()) {
            this->collectResultRecursion(snVisited, it);
        }
    }

    void TIAnalysis::insnSemOP1SetFloat(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 1)
                FIG_PANIC("op_count != 1"););
        const cs_x86_op *operands = x86Detail.operands;
        auto op0                  = this->OPFactory.getTypeInferOPByX86OP(operands[0]);

        switch(op0->size) {
        case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float32; break;
        case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float64; break;
        case 10: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float64; break;
        case 16: ctx[op0] = CFG::VTypeFactory::baseVType.bt_float128; break;
        default: FIG_PANIC("op.size != 4 || 8 || 16, op.size=", static_cast<uint16_t>(op0->size));
        }
    }

    void TIAnalysis::insnSemOP1SetSignedInt(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 1)
                FIG_PANIC("op_count != 1"););
        const cs_x86_op *operands = x86Detail.operands;
        auto op0                  = this->OPFactory.getTypeInferOPByX86OP(operands[0]);

        switch(op0->size) {
        case 2: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int16; break;
        case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int32; break;
        case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_int64; break;
        default: FIG_PANIC("op.size != 2 || 4 || 8, op.size=", static_cast<uint16_t>(op0->size));
        }
    }

    void TIAnalysis::insnSemOP1SetUnsignedInt(TIContext &ctx, const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        FIG_DEBUG(
            if(x86Detail.op_count != 1)
                FIG_PANIC("op_count != 1"););
        const cs_x86_op *operands = x86Detail.operands;
        auto op0                  = this->OPFactory.getTypeInferOPByX86OP(operands[0]);

        switch(op0->size) {
        case 2: ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint16; break;
        case 4: ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint32; break;
        case 8: ctx[op0] = CFG::VTypeFactory::baseVType.bt_uint64; break;
        default: FIG_PANIC("op.size != 2 || 4 || 8, op.size=", static_cast<uint16_t>(op0->size));
        }
    }

    void TIAnalysis::insnSemOP0NOP([[maybe_unused]] const cs_insn &insn) {
        FIG_DEBUG(
            const auto &x86Detail = insn.detail->x86;
            if(x86Detail.op_count != 0)
                FIG_PANIC("op_count=", static_cast<uint16_t>(x86Detail.op_count)););

        /* Empty */
        return;
    }

    void TIAnalysis::insnSemOP1NOP([[maybe_unused]] const cs_insn &insn) {
        FIG_DEBUG(
            const auto &x86Detail = insn.detail->x86;
            if(x86Detail.op_count != 1)
                FIG_PANIC("op_count=", static_cast<uint16_t>(x86Detail.op_count)););

        /* Empty */
        return;
    }

    void TIAnalysis::insnSemOP2NOP([[maybe_unused]] const cs_insn &insn) {
        FIG_DEBUG(
            const auto &x86Detail = insn.detail->x86;
            if(x86Detail.op_count != 2)
                FIG_PANIC("op_count=", static_cast<uint16_t>(x86Detail.op_count)););

        /* Empty */
        return;
    }

    void TIAnalysis::insnSemOP3NOP([[maybe_unused]] const cs_insn &insn) {
        FIG_DEBUG(
            const auto &x86Detail = insn.detail->x86;
            if(x86Detail.op_count != 3)
                FIG_PANIC("op_count=", static_cast<uint16_t>(x86Detail.op_count)););

        /* Empty */
        return;
    }

    void TIAnalysis::insnSemRemoveReg(TIContext &ctx, x86_reg reg) {
        auto it = ctx.begin();
        while(it != ctx.end()) {
            switch(it->first->type) {
            case X86_OP_IMM: {
                it = std::next(it);
                break;
            }
            case X86_OP_REG: {
                if(it->first->detail.reg == reg)
                    it = ctx.erase(it);
                else
                    it = std::next(it);
                break;
            }
            case X86_OP_MEM: {
                auto &x86OPMem = it->first->detail.mem;
                if(x86OPMem.base == reg || x86OPMem.index == reg || x86OPMem.segment == reg)
                    it = ctx.erase(it);
                else
                    it = std::next(it);
                break;
            }
            default: FIG_PANIC("something error!");
            }
        }
    }

    void TIAnalysis::insnSemRemoveOP(TIContext &ctx, TIOperandRef op) {
        if(op->type == X86_OP_REG) {
            this->insnSemRemoveReg(ctx, op->detail.reg);
        } else {
            auto pos = ctx.find(op);
            if(pos != ctx.end())
                ctx.erase(pos);
        }
    }

    void TIAnalysis::initializeCFA() {
        CFA initCFA = {
            .rspCFA = {
                .segment = X86_REG_INVALID,
                .base    = X86_REG_RSP,
                .index   = X86_REG_INVALID,
                .scale   = 1,
                .disp    = 8,
            },
            .rbpCFA = {
                .segment = X86_REG_INVALID,
                .base    = X86_REG_INVALID,
                .index   = X86_REG_INVALID,
                .scale   = 1,
                .disp    = 0,
            }};

        uint32_t entryBlockSn = this->func->cfg.value().getEntryBlock()->sn;

        std::set<uint32_t> visitedSnSet{};
        this->initializeCFARecursion(visitedSnSet, initCFA, entryBlockSn);
    }

    void TIAnalysis::initializeCFARecursion(std::set<uint32_t> &snVisited, CFA &cfa, uint32_t blockSn) {
        if(snVisited.contains(blockSn))
            return;
        snVisited.insert(blockSn);

        FIG_DEBUG(
            if(this->func->cfg->blocks.find(blockSn) == this->func->cfg->blocks.end())
                FIG_PANIC("stop, blockSn:", blockSn);
            if(this->blockInsnCFA.find(blockSn) == this->blockInsnCFA.end())
                FIG_PANIC("stop, blockSn:", blockSn););

        auto &curBlock     = this->func->cfg->blocks[blockSn];
        auto &blockInsnCFA = this->blockInsnCFA[blockSn];
        for(uint32_t i = 0; i < curBlock->statements.size(); i++) {
            blockInsnCFA[i] = cfa;
            this->initializeCFAForInsn(blockInsnCFA[i], curBlock->statements[i], cfa);
        }

        for(auto &succ : curBlock->getSuccsExceptCall()) {
            CFA cfaCopy = cfa;
            this->initializeCFARecursion(snVisited, cfaCopy, succ->sn);
        }
    }

    void TIAnalysis::initializeCFAForInsn(const CFA &precfa, const cs_insn &insn, CFA &postcfa) {
        // push rbp; || push REG;
        // pop rbp; || pop REG;
        // mov rbp, rsp;
        // ret;
        // leave;
        // sub rsp, 0x10;
        // add rsp, 0x10;
        postcfa = precfa;
        if(insn.id == X86_INS_PUSH) {
            // push rbp; || push REG
            postcfa.rspCFA.disp = postcfa.rspCFA.disp + 8;
        } else if(insn.id == X86_INS_MOV && insn.detail->x86.op_count == 2 && insn.detail->x86.operands[0].type == X86_OP_REG && insn.detail->x86.operands[0].reg == X86_REG_RBP && insn.detail->x86.operands[1].type == X86_OP_REG && insn.detail->x86.operands[1].reg == X86_REG_RSP) {
            // mov rbp, rsp;
            postcfa.rbpCFA      = postcfa.rspCFA;
            postcfa.rbpCFA.base = X86_REG_RBP;
        } else if(insn.id == X86_INS_LEAVE) {
            // leava; # mov rsp, rbp; pop rbp;
            postcfa.rspCFA      = postcfa.rbpCFA;
            postcfa.rspCFA.base = X86_REG_RSP;
            postcfa.rspCFA.disp = postcfa.rspCFA.disp - 8;
            postcfa.rbpCFA.base = X86_REG_INVALID;
        } else if(insn.id == X86_INS_POP) {
            // pop REG;
            postcfa.rspCFA.disp = postcfa.rspCFA.disp - 8;
            // pop rbp; 特殊情况
            if(insn.detail->x86.op_count == 1 && insn.detail->x86.operands[0].type == X86_OP_REG && insn.detail->x86.operands[0].reg == X86_REG_RBP) {
                postcfa.rbpCFA.base = X86_REG_INVALID;
            }
        } else if(insn.id == X86_INS_RET) {
            // ret; # pop eip;
            postcfa.rspCFA.disp = postcfa.rspCFA.disp - 8;
        } else if(insn.id == X86_INS_SUB && insn.detail->x86.op_count == 2 && insn.detail->x86.operands[0].type == X86_OP_REG && insn.detail->x86.operands[0].reg == X86_REG_RSP && insn.detail->x86.operands[1].type == X86_OP_IMM) {
            // sub rsp, 0x10;
            postcfa.rspCFA.disp = postcfa.rspCFA.disp + insn.detail->x86.operands[1].imm;
        } else if(insn.id == X86_INS_ADD && insn.detail->x86.op_count == 2 && insn.detail->x86.operands[0].type == X86_OP_REG && insn.detail->x86.operands[0].reg == X86_REG_RSP && insn.detail->x86.operands[1].type == X86_OP_IMM) {
            // add rsp, 0x10;
            postcfa.rspCFA.disp = postcfa.rspCFA.disp - insn.detail->x86.operands[1].imm;
        }
    }

    nlohmann::json TIAnalysis::toJson(bool withBlockContext, bool withVars, bool withSeletedContext, bool withCFA) {
        std::vector<std::string> asmList(this->func->numOfInsn);
        auto curBlock            = this->func->cfg->getEntryBlock();
        uint32_t globalInsnIndex = 0;
        while(curBlock->previous_block.has_value()) {
            curBlock = curBlock->previous_block.value();
            for(uint32_t i = 0; i < curBlock->statements.size(); i++) {
                FIG_DEBUG(
                    if(!this->blockInsnContexts.contains(curBlock->sn)) {
                        FIG_PANIC("stop, blockSn:", curBlock->sn);
                    });

                auto &insn = curBlock->statements[i];
                std::string contextStr;
                if(withSeletedContext) {
                    // selected
                    if(isIrrelevantInsn(insn)) {
                        contextStr = "Irrelevant Insn";
                    } else {
                        for(uint32_t opnum = 0; opnum < insn.detail->x86.op_count; opnum++) {
                            auto &csX86OP    = insn.detail->x86.operands[opnum];
                            auto typeInferOP = this->OPFactory.getTypeInferOPByX86OP(csX86OP);
                            if(typeInferOP->type == X86_OP_IMM) {
                                contextStr = contextStr + "{" + typeInferOP->toString() + " : IMM}";
                            } else if(this->blockInsnContexts[curBlock->sn][i].contains(typeInferOP)) {
                                auto vType = this->blockInsnContexts[curBlock->sn][i][typeInferOP];
                                contextStr = contextStr + "{" + typeInferOP->toString() + " : " + vType->name + "}";
                            } else {
                                contextStr = contextStr + "{" + typeInferOP->toString() + " : unknown}";
                            }
                        }
                    }
                } else {
                    // full
                    for(const auto &[insnOP, OPVType] : this->blockInsnContexts[curBlock->sn][i]) {
                        contextStr = contextStr + "{" + insnOP->toString() + " : " + OPVType->name + "}";
                    }
                }
                asmList[globalInsnIndex] = CFG::insnToString(insn) + "{" + contextStr + "}";
                if(withCFA) {
                    FIG_DEBUG(
                        if(!this->blockInsnCFA.contains(curBlock->sn))
                            FIG_PANIC("stop, blockSn:", curBlock->sn););

                    if(this->blockInsnCFA[curBlock->sn][i].rspCFA.base == X86_REG_RSP) {
                        std::string rspCFAStr;
                        rspCFAStr                = CFG::x86_op_mem_to_string(this->blockInsnCFA[curBlock->sn][i].rspCFA);
                        asmList[globalInsnIndex] = asmList[globalInsnIndex] + "{rspCFA : " + rspCFAStr + "}";
                    }
                    if(this->blockInsnCFA[curBlock->sn][i].rbpCFA.base == X86_REG_RBP) {
                        std::string rbpCFAStr;
                        rbpCFAStr                = CFG::x86_op_mem_to_string(this->blockInsnCFA[curBlock->sn][i].rbpCFA);
                        asmList[globalInsnIndex] = asmList[globalInsnIndex] + "{rbpCFA : " + rbpCFAStr + "}";
                    }
                }
                globalInsnIndex++;
            }
        }

        nlohmann::json content = {
            {"Statements", nlohmann::json(asmList)},
        };
        content["name"] = this->func->dwarfName;
        if(withVars) {
            content["Vars"] = nlohmann::json::array();
            for(auto &v : this->func->variableList) {
                content["Vars"].push_back(v.toJson());
            }
        }
        if(withBlockContext) {
            content["InContexts"] = nlohmann::json::array();
            for(auto &it : this->inBlockContexts) {
                nlohmann::json blockContextNjson;
                blockContextNjson["sn"]   = it.first;
                blockContextNjson["addr"] = "0x" + CFG::dec_to_hex(this->func->cfg->blocks[it.first]->startAddr);
                std::string contextStr;
                for(auto &ctx : it.second) {
                    contextStr = contextStr + "{" + ctx.first->toString() + " : " + ctx.second->name + "}";
                }
                blockContextNjson["context"] = "{" + contextStr + "}";
                content["InContexts"].push_back(blockContextNjson);
            }
            content["OutContexts"] = nlohmann::json::array();
            for(auto &it : this->inBlockContexts) {
                nlohmann::json blockContextNjson;
                blockContextNjson["sn"]   = it.first;
                blockContextNjson["addr"] = "0x" + CFG::dec_to_hex(this->func->cfg->blocks[it.first]->startAddr);
                std::string contextStr;
                for(auto &ctx : it.second) {
                    contextStr = contextStr + "{" + ctx.first->toString() + " : " + ctx.second->name + "}";
                }
                blockContextNjson["context"] = "{" + contextStr + "}";
                content["OutContexts"].push_back(blockContextNjson);
            }
        }

        return content;
    }

    CFG::VTypePointer TIAnalysis::getVTypeWithContext(const TIOperandRef operand, TIContext &ctx) {
        if(ctx.contains(operand))
            return ctx[operand];

        if(this->config.enableMemOPCalculatedAnalysis) {
            if(operand->type != X86_OP_MEM)
                return nullptr;

            const auto &memDetial = operand->detail.mem;
            const auto baseRegOP  = this->OPFactory.getRegOPByx86Reg(memDetial.base);
            if(!ctx.contains(baseRegOP))
                return nullptr;

            const auto baseRegVType = ctx[baseRegOP];
            if(baseRegVType->typeTag != CFG::TYPETAG::POINTER)
                return nullptr;

            const auto refVType = baseRegVType->getRefVType();
            if(refVType && (refVType->typeTag == CFG::TYPETAG::POINTER || refVType->typeTag == CFG::TYPETAG::BASE)) {
                return refVType;
            } else if(refVType && (refVType->typeTag == CFG::TYPETAG::STRUCTURE || refVType->typeTag == CFG::TYPETAG::UNION || refVType->typeTag == CFG::TYPETAG::ARRAY)) {
                if(!(memDetial.segment == X86_REG_INVALID && memDetial.index == X86_REG_INVALID && memDetial.disp < refVType->size))
                    return nullptr;

                for(const auto &[pos, vType] : refVType->memoryModel) {
                    if(memDetial.disp == pos) {
                        return vType;
                    }
                }
            }
        }

        return nullptr;
    }

    CFG::VTypePointer TIAnalysis::getVTypeWithKnownVars(const CFA &cfa, const TIOperandRef typeInferOP, const cs_insn &insn, bool leaMode) {
        if(typeInferOP->type != X86_OP_MEM)
            return nullptr;

        const auto &OPMem = typeInferOP->detail.mem;
        if(this->config.enableGlobalVariablAnalysis && OPMem.base == X86_REG_RIP && OPMem.index == X86_REG_INVALID) {
            uint64_t targetAddr = insn.address + insn.size + OPMem.disp;
            uint32_t targetSize = typeInferOP->size;
            for(const auto &globalVar : *(this->globalVars)) {
                const auto vType      = globalVar->vType;
                const auto globalAddr = globalVar->addr.globalAddr;
                if(leaMode && targetAddr == globalAddr)
                    return this->typeCalculator.getPointerVType(vType);

                for(auto &[posIt, vTypeIt] : vType->memoryModel) {
                    auto tempAddr = globalAddr + posIt;
                    auto tempSize = vTypeIt->size;
                    if(leaMode && targetAddr == tempAddr)
                        return this->typeCalculator.getPointerVType(vType);

                    if(!leaMode && targetAddr == tempAddr && targetSize == tempSize)
                        return vTypeIt;
                }
            }
        }

        if(OPMem.base == X86_REG_RBP) {
            auto lambdaStackVar = [](const CFG::Var &v) {
                return (v.addr.type == CFG::ADDR_TYPE::STACK);
            };
            for(const auto &var : this->knownVars | std::views::filter(lambdaStackVar)) {
                const CFG::VarAddr &vAddr = var.addr;
                CFG::VTypePointer vType   = var.vType;
                cs_x86_op varBaseOP;
                varBaseOP.type     = X86_OP_MEM;
                varBaseOP.mem      = cfa.rbpCFA;
                varBaseOP.size     = static_cast<uint8_t>(vType->size);
                varBaseOP.mem.disp = varBaseOP.mem.disp + vAddr.CFAOffset;

                if(leaMode && OPMem.disp == varBaseOP.mem.disp && OPMem.index == X86_REG_INVALID)
                    return this->typeCalculator.getPointerVType(vType);

                for(const auto &[posIt, vTypeIt] : vType->memoryModel) {
                    cs_x86_op tempOP = varBaseOP;
                    tempOP.size      = vTypeIt->size;
                    tempOP.mem.disp  = tempOP.mem.disp + posIt;
                    if(leaMode && OPMem.disp == tempOP.mem.disp && OPMem.index == X86_REG_INVALID) {
                        return this->typeCalculator.getPointerVType(vTypeIt);
                    } else if(!leaMode) {
                        TIOperandRef curTypeInferOP = this->OPFactory.getTypeInferOPByX86OP(tempOP);
                        if(curTypeInferOP == typeInferOP)
                            return vTypeIt;
                    }
                }
            }
        }

        return nullptr;
    }

    CFG::VTypePointer TIAnalysis::getVTypeWithUnknownVars(const CFA &cfa, const TIOperandRef typeInferOP, bool leaMode) {
        if(!this->config.enableUsingUnknownVar)
            return nullptr;
        if(typeInferOP->type != X86_OP_MEM)
            return nullptr;

        const auto &OPMem = typeInferOP->detail.mem;
        if(OPMem.base == X86_REG_RBP) {
            auto lambdaUnknownAndStackVar = [this](const CFG::Var &v) {
                bool flagUnknown = (!this->knownVarsId.contains(v.varId));
                bool flagStack   = (v.addr.type == CFG::ADDR_TYPE::STACK);
                return flagUnknown && flagStack;
            };
            // For Parameter
            for(auto &var : this->func->parameterList | std::views::filter(lambdaUnknownAndStackVar)) {
                const CFG::VarAddr &vAddr = var.addr;
                CFG::VTypePointer vType   = var.vType;
                cs_x86_op varBaseOP;
                varBaseOP.type     = X86_OP_MEM;
                varBaseOP.mem      = cfa.rbpCFA;
                varBaseOP.size     = static_cast<uint8_t>(vType->size);
                varBaseOP.mem.disp = varBaseOP.mem.disp + vAddr.CFAOffset;
                if(leaMode && OPMem.disp == varBaseOP.mem.disp && OPMem.index == X86_REG_INVALID) {
                    var.isUsed = true;
                    this->knownVars.push_back(var);
                    this->knownVarsId.insert(var.varId);
                    return this->typeCalculator.getPointerVType(vType);
                }
                for(const auto &[posIt, vTypeIt] : vType->memoryModel) {
                    cs_x86_op tempOP = varBaseOP;
                    tempOP.size      = vTypeIt->size;
                    tempOP.mem.disp  = tempOP.mem.disp + posIt;
                    if(leaMode && OPMem.disp == tempOP.mem.disp && OPMem.index == X86_REG_INVALID) {
                        var.isUsed = true;
                        this->knownVars.push_back(var);
                        this->knownVarsId.insert(var.varId);
                        return this->typeCalculator.getPointerVType(vTypeIt);
                    } else if(!leaMode) {
                        TIOperandRef curTypeInferOP = this->OPFactory.getTypeInferOPByX86OP(tempOP);
                        if(curTypeInferOP == typeInferOP) {
                            var.isUsed = true;
                            this->knownVars.push_back(var);
                            this->knownVarsId.insert(var.varId);
                            return vTypeIt;
                        }
                    }
                }
            }
            // For Variable
            for(auto &var : this->func->variableList | std::views::filter(lambdaUnknownAndStackVar)) {
                const CFG::VarAddr &vAddr = var.addr;
                CFG::VTypePointer vType   = var.vType;
                cs_x86_op varBaseOP;
                varBaseOP.type     = X86_OP_MEM;
                varBaseOP.mem      = cfa.rbpCFA;
                varBaseOP.size     = static_cast<uint8_t>(vType->size);
                varBaseOP.mem.disp = varBaseOP.mem.disp + vAddr.CFAOffset;
                if(leaMode && OPMem.disp == varBaseOP.mem.disp && OPMem.index == X86_REG_INVALID) {
                    var.isUsed = true;
                    this->knownVars.push_back(var);
                    this->knownVarsId.insert(var.varId);
                    return this->typeCalculator.getPointerVType(vType);
                }
                for(const auto &[posIt, vTypeIt] : vType->memoryModel) {
                    cs_x86_op tempOP = varBaseOP;
                    tempOP.size      = vTypeIt->size;
                    tempOP.mem.disp  = tempOP.mem.disp + posIt;
                    if(leaMode && OPMem.disp == tempOP.mem.disp && OPMem.index == X86_REG_INVALID) {
                        var.isUsed = true;
                        this->knownVars.push_back(var);
                        this->knownVarsId.insert(var.varId);
                        return this->typeCalculator.getPointerVType(vTypeIt);
                    } else if(!leaMode) {
                        TIOperandRef curTypeInferOP = this->OPFactory.getTypeInferOPByX86OP(tempOP);
                        if(curTypeInferOP == typeInferOP) {
                            var.isUsed = true;
                            this->knownVars.push_back(var);
                            this->knownVarsId.insert(var.varId);
                            return vTypeIt;
                        }
                    }
                }
            }
        }

        return nullptr;
    }

    bool TIAnalysis::unionTIContext(TIContext &baseContext, const TIContext &addtionalContext) {
        TIContext tempContext = baseContext;
        for(auto &it : addtionalContext) {
            auto &[typeInferOP, vType] = it;
            if(tempContext.contains(typeInferOP)) {
                CFG::VTypePointer v1     = tempContext[typeInferOP];
                CFG::VTypePointer vunion = this->typeCalculator.getUnionVType(v1, vType);
                tempContext.insert({typeInferOP, vunion});
            } else {
                tempContext.insert(it);
            }
        }

        if(baseContext == tempContext) {
            return false;
        } else {
            baseContext = tempContext;
            return true;
        }
    }

    bool TIAnalysis::isSavingRegInsn(const cs_insn &insn) {
        const auto &x86Detail = insn.detail->x86;
        const auto operands   = x86Detail.operands;
        switch(insn.id) {
        case X86_INS_PUSH: {
            // push [callingReg];
            if(operands[0].type == X86_OP_REG && isCallingRegExceptRspRbp(operands[0].reg))
                return true;

            return false;
        }
        default: return false;
        }
    }

    bool TIAnalysis::isIrrelevantInsn(const cs_insn &insn) {
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
        case X86_INS_JS: {
            return true;
        }
        case X86_INS_CALL: {
            return true;
        }
        case X86_INS_PUSH: {
            // push rsp; push rbp; push [callingReg]
            if(operands[0].type == X86_OP_REG && isCallingReg(operands[0].reg))
                return true;

            return false;
        }
        case X86_INS_POP: {
            // pop rsp; push rbp;
            if(operands[0].type == X86_OP_REG && (operands[0].reg == X86_REG_RBP || operands[0].reg == X86_REG_RSP))
                return true;

            return false;
        }
        case X86_INS_MOV: {
            if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_REG) {
                if(operands[0].reg == X86_REG_RBP && operands[1].reg == X86_REG_RSP) // mov rbp, rsp;
                    return true;
                else if(operands[0].reg == X86_REG_RSP && operands[1].reg == X86_REG_RBP) // mov rsp, rbp;
                    return true;
            }
            return false;
        }
        case X86_INS_SUB:   // sub esp, 10
        case X86_INS_ADD: { // add esp, 10
            if(operands[0].type == X86_OP_REG && (operands[0].reg == X86_REG_RSP || operands[0].reg == X86_REG_RBP))
                return true;
            else
                return false;
        }
        case X86_INS_XOR: { // xor eax, eax
            if(operands[0].type == X86_OP_REG && operands[1].type == X86_OP_REG && operands[0].reg == operands[1].reg)
                return true;
            else
                return false;
        }
        default: return false;
        }
    }

    void TIAnalysis::clearCalledOP(TIContext &ctx) {
        auto it = ctx.begin();
        while(it != ctx.end()) {
            const auto &[OP, _]         = *it;
            const auto type             = OP->type;
            const auto detail           = OP->detail;
            const auto invaildOrCalling = [](x86_reg reg) { return (reg == X86_REG_INVALID || isCallingReg(reg)); };
            if(type == X86_OP_REG && !isCallingReg(detail.reg)) {
                it = ctx.erase(it);
            } else if(type == X86_OP_MEM && !(invaildOrCalling(detail.mem.base) && invaildOrCalling(detail.mem.index))) {
                it = ctx.erase(it);
            } else {
                it = std::next(it);
            }
        }
    }

    bool TIAnalysis::isCallingRegExceptRspRbp(x86_reg reg) {
        switch(reg) {
        // RBX
        case X86_REG_BL: return true;
        case X86_REG_BH: return true;
        case X86_REG_BX: return true;
        case X86_REG_EBX: return true;
        case X86_REG_RBX: return true;
        // R12
        case X86_REG_R12B: return true;
        case X86_REG_R12W: return true;
        case X86_REG_R12D: return true;
        case X86_REG_R12: return true;
        // R13
        case X86_REG_R13B: return true;
        case X86_REG_R13W: return true;
        case X86_REG_R13D: return true;
        case X86_REG_R13: return true;
        // R14
        case X86_REG_R14B: return true;
        case X86_REG_R14W: return true;
        case X86_REG_R14D: return true;
        case X86_REG_R14: return true;
        // R15
        case X86_REG_R15B: return true;
        case X86_REG_R15W: return true;
        case X86_REG_R15D: return true;
        case X86_REG_R15: return true;
        default: return false;
        }
    }

    bool TIAnalysis::isCallingReg(x86_reg reg) {
        switch(reg) {
        // RBX
        case X86_REG_BL: return true;
        case X86_REG_BH: return true;
        case X86_REG_BX: return true;
        case X86_REG_EBX: return true;
        case X86_REG_RBX: return true;
        // RBP
        case X86_REG_BPL: return true;
        case X86_REG_BP: return true;
        case X86_REG_EBP: return true;
        case X86_REG_RBP: return true;
        // RSP
        case X86_REG_SPL: return true;
        case X86_REG_SP: return true;
        case X86_REG_ESP: return true;
        case X86_REG_RSP: return true;
        // R12
        case X86_REG_R12B: return true;
        case X86_REG_R12W: return true;
        case X86_REG_R12D: return true;
        case X86_REG_R12: return true;
        // R13
        case X86_REG_R13B: return true;
        case X86_REG_R13W: return true;
        case X86_REG_R13D: return true;
        case X86_REG_R13: return true;
        // R14
        case X86_REG_R14B: return true;
        case X86_REG_R14W: return true;
        case X86_REG_R14D: return true;
        case X86_REG_R14: return true;
        // R15
        case X86_REG_R15B: return true;
        case X86_REG_R15W: return true;
        case X86_REG_R15D: return true;
        case X86_REG_R15: return true;
        default: return false;
        }
    }

} // namespace SA
