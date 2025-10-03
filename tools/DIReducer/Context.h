#ifndef DR_CONTEXT_H
#define DR_CONTEXT_H

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "CFG/AsmCFG.h"
#include "CFG/CG.h"
#include "CFG/Type.h"
#include "DWARF/DWARFObject.h"
#include "DWARF/ELFObject.h"
#include "DWARF/LineTable.h"
#include "DWARFLinker/Linker.h"
#include "Config.h"
#include "LibLoader.h"
#include "Core/RuntimeInfo.h"
#include "libdwarf/dwarf.h"
#include "nlohmann/json_fwd.hpp"

namespace CFG {
    struct Func;
    struct Var;
} // namespace CFG

struct AbbrevList;
struct CompilationUnit;
struct InfoEntry;

using GenaralTimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
struct DWARFCompressionInfo {
    GenaralTimePoint allStartTime;
    GenaralTimePoint allEndTime;
    GenaralTimePoint DISStartTime;
    GenaralTimePoint DISEndTime;
    GenaralTimePoint TIStartTime;
    GenaralTimePoint TIEndTime;
    GenaralTimePoint SPStartTime1;
    GenaralTimePoint SPEndTime1;
    GenaralTimePoint SPStartTime2;
    GenaralTimePoint SPEndTime2;
    GenaralTimePoint BRStartTime;
    GenaralTimePoint BREndTime;
    int64_t symbolsSize;
    int64_t inputFileSizeWithoutSymols;
    int64_t inputDWARFSize;
    int64_t outputFileSizeWithoutSymbols;
    int64_t outputDWARFSize;
    int64_t maxMemUsage;
    int64_t allAttrsCount;
    double AllTimeCost;
    double DISTimeCost;
    double TITimeCost;
    double SPTimeCost;
    double BRTimeCost;
    double OtherTimeCost;
};

struct GenaralInfo {
    GenaralTimePoint startTime;
    GenaralTimePoint endTime;
    double timeCost;
};

class DIReducerContext {
private:
    Config config;
    // source
    ELFObjetc elfObject;
    DWARFObject dwarfObject;
    // target
    ELFObjetc dstElfObject;
    DWARFObject dstDwarfObject;

    std::optional<DWARFLinker::Linker> linkerOpt;

    // debug info string table
    StrTable strTable;

    // line table
    LineTable lineTable;

    // relocation info
    std::unordered_map<int64_t, int64_t> GlobalOffsetMap;
    std::vector<std::unordered_map<int64_t, int64_t>> DIEOffsetMapVec;
    std::unordered_map<uint64_t, uint64_t> CUOffsetMap;

    // functions
    std::vector<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> funcDataBase;

    // types
    std::vector<std::unordered_map<uint64_t, CFG::GTypeInfo>> gTypeInfoDataBase;
    std::vector<std::unordered_map<uint64_t, std::shared_ptr<CFG::VTypeInfo>>> vTypeInfoDataBase;

    // reduced variable id
    std::unordered_set<uint64_t> redundantVarIdSet;

    // for dyn-lib function
    DynLibLoader dynLibLoader;
    std::unordered_map<std::string, std::shared_ptr<CFG::Func>> dynLibDataBase;
    std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> dynLibFuncMap;

    // function address
    std::shared_ptr<std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>>> customFuncMap;
    std::shared_ptr<std::vector<std::shared_ptr<CFG::Var>>> globalVars;

    nlohmann::json njson;
    DWARFCompressionInfo coreInfo;
    CFG::CFGConstructionInfo cfgInfo;
    SA::RuntimeInfo tiRuntimeInfo;
    CFG::CGConstructionInfo cgInfo;
    CFG::CG cg;

private:
    // <EntryAddr, <VarName, varType>>
    std::map<uint64_t, std::pair<std::string, uint64_t>> absVarsInfo;

    // <EntryAddr, <FuncName, RetType>>
    std::map<uint64_t, std::pair<std::string, uint64_t>> absFuncsInfo;

private:
    void saveJsonToFile(const std::string &targetFilePath, const nlohmann::json &jsonObject);

    void convertCFGToDot(const std::string &targetDir);
    void convertCGToDot(const std::string &targetDir);

    void initSrc();
    void initDst();
    void initSPA();

    void collectInputInfo();

    void initLink();
    void initDebugStrTable();

    void initDynLibLoader();
    void initDynLibDataBase();
    void initDynLibFuncMap();

    void initGTypeInfo();
    void initGTypeInfoInCompilationUnit(CompilationUnit &cu, std::unordered_map<uint64_t, CFG::GTypeInfo> &generalTypeMap);
    void initGTypeInfoInInfoEntry(const InfoEntry &infoEntry, std::unordered_map<uint64_t, CFG::GTypeInfo> &generalTypeMap);

    void initAbsVarInfo();
    void initAbsVarInfoInCompilationUnit(CompilationUnit &cu);
    void initAbsVarInfoInInfoEntry(InfoEntry &infoEntry);

    void initAbsFuncInfo();
    void initAbsFuncInfoInCompilationUnit(CompilationUnit &cu);
    void initAbsFuncInfoInInfoEntry(InfoEntry &infoEntry);

    void initGlobalVars();
    void initGlobalVarsInCompilationUnit(CompilationUnit &cu);

    void initVTypeFactory();
    void initVTypeInfo();

    void initFuncDesc();
    void initFuncDescInCompilationUnit(CompilationUnit &cu, std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>> &funcMap);
    void initFuncDescInInfoEntry(InfoEntry &infoEntry, std::unordered_map<uint64_t, std::shared_ptr<CFG::Func>> &funcMap);
    void fillFuncDescInInfoEntry(std::shared_ptr<CFG::Func> func, InfoEntry &infoEntry, uint64_t low_pc, uint64_t high_pc);
    void initCustomFuncMap();
    void initFuncASM();
    void initFuncCFG();
    void initProgramCFG();
    void initCG();

    CFG::GTypeInfo getSingleType(const InfoEntry &infoEntry);
    CFG::GTypeInfo getMultiType(const InfoEntry &infoEntry, std::unordered_map<uint64_t, CFG::GTypeInfo> &generalTypeMap);

    // void reduceCompilationUnit(CompilationUnit &cu);
    // void reduceInfoEntry(InfoEntry &infoEntry, std::shared_ptr<AbbrevList> &abbrevList);

    nlohmann::json getVTypeInfoJson();
    nlohmann::json getFuncTypeMapJson();
    std::shared_ptr<CFG::VTypeInfo> getVTypeInfo(uint64_t type, uint16_t module);

private:
    void executeTypeInference();

public:
    nlohmann::json getFunctionsJson(bool withHeader, bool withInsns, bool withVars);
    nlohmann::json getCoreInfo();
    nlohmann::json getBaseInfo();

public:
    DIReducerContext(const Config &config) :
        config(config) {
    }

    ~DIReducerContext() = default;

    void run();
    void dump();
    // void reduce();

    // ref: dwarf5.pdf p207 p252
    const std::set<uint64_t> remove_attribute_set = {
        DW_AT_accessibility,
        DW_AT_artificial,
        DW_AT_associated,
        DW_AT_call_column,
        DW_AT_call_file,
        DW_AT_call_line,
        DW_AT_comp_dir,
        DW_AT_const_expr,
        DW_AT_const_value,
        DW_AT_decimal_scale,
        DW_AT_decimal_sign,
        DW_AT_decl_column,
        DW_AT_decl_file,
        DW_AT_decl_line,
        DW_AT_declaration,
        DW_AT_defaulted,
        DW_AT_default_value,
        DW_AT_deleted,
        DW_AT_description,
        DW_AT_digit_count,
        DW_AT_dwo_id,
        DW_AT_dwo_name,
        DW_AT_elemental,
        DW_AT_explicit,
        DW_AT_export_symbols,
        DW_AT_extension,
        DW_AT_friend,
        DW_AT_identifier_case,
        DW_AT_inline,
        DW_AT_is_optional,
        DW_AT_language,
        DW_AT_linkage_name,
        DW_AT_mutable,
        DW_AT_macro_info,
        DW_AT_macros,
        DW_AT_main_subprogram,
        DW_AT_mutable,
        DW_AT_name,
        DW_AT_namelist_item,
        DW_AT_noreturn,
        DW_AT_picture_string,
        DW_AT_priority,
        DW_AT_producer,
        DW_AT_prototyped,
        DW_AT_pure,
        DW_AT_recursive,
        DW_AT_reference,
        DW_AT_rvalue_reference,
        DW_AT_segment,
        DW_AT_sibling,
        DW_AT_specification,
        DW_AT_start_scope,
        DW_AT_stmt_list,
        DW_AT_str_offsets_base,
        DW_AT_use_UTF8,
        DW_AT_virtuality,
        DW_AT_visibility,
    };
};

#endif