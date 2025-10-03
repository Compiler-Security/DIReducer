#ifndef TI_ANALYSIS_H
#define TI_ANALYSIS_H

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <capstone/x86.h>
#include <capstone/capstone.h>
#include "TIConfigure.h"
#include "RuntimeInfo.h"
#include "TypeCalculator.h"
#include "TIContext.h"
#include "TIOperand.h"
#include "CFG/Func.h"
#include "CFG/Type.h"
#include "CFG/Var.h"
#include "DepAnalysis.h"

namespace SA {
    struct CFA {
        x86_op_mem rspCFA;
        x86_op_mem rbpCFA;
    };

    struct TypeCalculator;
    struct TIAnalysis;
} // namespace SA

struct SA::TIAnalysis {
public:
    
    std::string programName;
    std::string funcName;
    RuntimeInfo runtimeInfo;
    TIConfigure config;

public:
    std::shared_ptr<CFG::Func> func;
    std::vector<CFG::Var> knownVars;
    std::unordered_set<uint64_t> knownVarsId;

private:
    TIOperandFactory OPFactory;
    TypeCalculator typeCalculator;
    std::unordered_map<uint32_t, std::vector<TIContext>> blockInsnContexts;
    std::unordered_map<uint32_t, TIContext> inBlockContexts;
    std::unordered_map<uint32_t, TIContext> outBlockContexts;
    std::unordered_map<uint32_t, std::vector<CFA>> blockInsnCFA;
    std::ofstream os;

public:
    DepGraph depGraph;
private:
    std::unordered_map<uint32_t, std::vector<DepContext>> blockInsnDepContexts;
    std::unordered_map<uint32_t, DepContext> inBlockDepContexts;
    std::unordered_map<uint32_t, DepContext> outBlockDepContexts;

private:
    std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> customFuncMap;
    std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> dynLibFuncMap;
    std::shared_ptr<std::vector<std::shared_ptr<CFG::Var>>> globalVars;

public:
    TIAnalysis() = delete;
    TIAnalysis(const std::string &name, const std::shared_ptr<CFG::Func> &f);
    ~TIAnalysis();

    void run();
    void collectResult();
    void savingResultOnFile();
    void parameterInference();
    std::unordered_set<uint64_t> getRedundantVarIdSet() const;
    void setFuncTypeMap(const std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> &funcTypeMap);
    void setDynLibFuncMap(const std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> &dynLibFuncMap);
    void setGlobalVars(const std::shared_ptr<std::vector<std::shared_ptr<CFG::Var>>> &globalVars);
    nlohmann::json toJson(bool withBlockContext, bool withVars, bool withSeletedContext, bool withCFA);

private:
    static bool isIrrelevantInsn(const cs_insn &insn);
    static bool isSavingRegInsn(const cs_insn &insn);
    static void clearCalledOP(TIContext &ctx);
    static bool isCallingReg(x86_reg reg);
    static bool isCallingRegExceptRspRbp(x86_reg reg);

private:
    void collectResultIterate();
    void collectResultRecursion(std::set<uint32_t> &snVisited, std::shared_ptr<CFG::AsmBlock> &block);
    void initializeCFA();
    void initializeCFARecursion(std::set<uint32_t> &snVisited, CFA &cfa, uint32_t blockSn);
    void initializeCFAForInsn(const CFA &precfa, const cs_insn &insn, CFA &postcfa);

    void depInit();
    void depFinish();
    void depAnalysis();
    void depIterativeExecution(std::set<uint32_t> &snVisited, std::shared_ptr<CFG::AsmBlock> &block);
    void depAnalysisInBlock(const std::shared_ptr<CFG::AsmBlock> &block);
    void depAnalysisInStatement(DepContext &insnINContext, const cs_insn &insn, const CFA &cfa);
    std::unordered_set<uint64_t> getDepAnalysisResult() const;
    std::optional<DepNode> getDepNode(TIOperandRef op, const CFA &cfa);
    void propagateDep(TIOperandRef src, TIOperandRef dst, DepContext& ctx, const CFA &cfa);
    void propagateDep(TIOperandRef src0, TIOperandRef src1, TIOperandRef dst, DepContext& ctx, const CFA &cfa);
    void propagateDep(TIOperandRef src0, TIOperandRef src1, TIOperandRef src2, TIOperandRef dst, DepContext& ctx, const CFA &cfa);
    bool unionDepContext(DepContext &dst, const DepContext &src);
    void removeRegFromDepContext(DepContext &ctx, x86_reg reg);

    void parameterIterativeExecution(std::set<uint32_t> &snVisited, const std::shared_ptr<CFG::AsmBlock> &block);
    void parameterInferenceInBlock(const std::shared_ptr<CFG::AsmBlock> &block);
    void parameterInferenceInStatement(TIContext &insnINContext, const cs_insn &insn);

    void forwardInference();
    void forwardIterativeExecution(std::set<uint32_t> &snVisited, std::shared_ptr<CFG::AsmBlock> &block);
    bool forwardInferenceInBlock(const std::shared_ptr<CFG::AsmBlock> &block);
    bool forwardInferenceInStatement(const TIContext &insnINContext, TIContext &insnOUTContext, const CFA &cfa, const cs_insn &insn);

    void backwardInference();
    void backwardIterativeExecution(std::set<uint32_t> &snVisited, std::shared_ptr<CFG::AsmBlock> &block);
    bool backwardInferenceInBlock(const std::shared_ptr<CFG::AsmBlock> &block);
    bool backwardInferenceInStatement(const TIContext &insnOUTContext, TIContext &insnINContext, const CFA &cfa, const cs_insn &insn);

    /// ***********************
    /// API For Type Operations
    /// ***********************

    bool unionTIContext(TIContext &baseContext, const TIContext &addtionalContext);
    CFG::VTypePointer getVTypeWithContext(const TIOperandRef operand, TIContext &ctx);
    CFG::VTypePointer getVTypeWithKnownVars(const CFA &cfa, const TIOperandRef typeInferOP, const cs_insn &insn, bool leaMode);
    CFG::VTypePointer getVTypeWithUnknownVars(const CFA &cfa, const TIOperandRef typeInferOP, bool leaMode);

    void insnSemForwardOP2Src123Dst13(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemForwardOP2LEA(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemForwardOP2MOVFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemForwardOP2ADDSUBFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemForwardOP2CMPFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemForwardOP2LOGICFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemForwardOP2MOVZX(TIContext &ctx, const cs_insn &insn);
    void insnSemForwardOP2MOVSX(TIContext &ctx, const cs_insn &insn);
    void insnSemForwardOP2MOVSXD(TIContext &ctx, const cs_insn &insn);
    void insnSemForwardOP1Call(TIContext &ctx, const cs_insn &insn);

    void insnSemBackwardOP2Dst1(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemBackwardOP2LEA(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemBackwardOP2MOVFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemBackwardOP2ADDSUBFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemBackwardOP2CMPFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemBackwardOP2LOGICFamily(TIContext &ctx, const cs_insn &insn, const CFA &cfa);
    void insnSemBackwardOP2MOVZX(TIContext &ctx, const cs_insn &insn);
    void insnSemBackwardOP2MOVSX(TIContext &ctx, const cs_insn &insn);
    void insnSemBackwardOP2MOVSXD(TIContext &ctx, const cs_insn &insn);
    void insnSemBackwardOP1Call(TIContext &ctx, const cs_insn &insn);
    void insnSemBackwardOP2EQ(TIContext &ctx, const cs_insn &insn);

    void insnSemOP1SetFloat(TIContext &ctx, const cs_insn &insn);
    void insnSemOP1SetSignedInt(TIContext &ctx, const cs_insn &insn);
    void insnSemOP1SetUnsignedInt(TIContext &ctx, const cs_insn &insn);
    void insnSemRemoveReg(TIContext &ctx, x86_reg reg);
    void insnSemRemoveOP(TIContext &ctx, TIOperandRef op);
    void insnSemOP0NOP(const cs_insn &insn);
    void insnSemOP1NOP(const cs_insn &insn);
    void insnSemOP2NOP(const cs_insn &insn);
    void insnSemOP3NOP(const cs_insn &insn);
};

#endif