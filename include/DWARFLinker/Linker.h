#ifndef DWARFLINKER_LINKER_H
#define DWARFLINKER_LINKER_H

#include <cstdint>                    // for uint64_t, uint32_t
#include <map>                        // for map
#include <memory>                     // for shared_ptr
#include <string>                     // for string
#include <sys/types.h>
#include <unordered_map>              // for unordered_map
#include <unordered_set>              // for unordered_set
#include <utility>                    // for pair
#include <vector>                     // for vector
#include "CFG/Type.h"
#include "DWARF/InfoObject.h"         // for InfoObject (ptr only), DWARFIn...
#include "DWARFLinker/FunctionCell.h" // for FunctionCell
#include "DWARFLinker/TypeCell.h"     // for TypeCell
#include "DWARFLinker/VariableCell.h" // for VariableCell

namespace CFG {
    struct VTypeInfo;
}
struct AbbrevObject;
struct InfoEntry;

namespace DWARFLinker {
    struct Linker;
    struct LinkInfo;
    enum class AbbrevNumBer {
        EndItem = 0,
        Producer,
        FuncWithSubNode,
        FuncWithoutSubNode,
        FuncVoidWithSubNode,
        FuncVoidWithoutSubNode,
        Variable,
        Parameter,
        BaseType,
        PointerType,
        ArrayType,
        VoidTYPE,
        StructTYPE,
        MemberTYPE,
        ByteLocMemberTYPE,
        BitLocMemberTYPE,
        UnionType,
        Block
    };
} // namespace DWARFLinker

struct DWARFLinker::LinkInfo {
    uint64_t redundantTypeAttrs;
    uint64_t redundantVarAttrs;

    nlohmann::json toJson() const {
        nlohmann::json content = {
            {"redundantTypeAttrs", this->redundantTypeAttrs},
            {"redundantVarAttrs", this->redundantVarAttrs},
        };
        return content;
    }
};

struct DWARFLinker::Linker {
    // Data members

    std::shared_ptr<InfoObject> srcInfoObject;
    std::shared_ptr<InfoObject> dstInfoObject;
    std::shared_ptr<AbbrevObject> dstAbbrevObject;

    // <Module, Offset, VecIndex>

    std::vector<std::map<uint64_t, uint64_t>> typeVecMap;

    // <Module, <offset>>
    std::vector<std::unordered_set<uint64_t>> redundantTypes;

    // Cells

    std::vector<DWARFLinker::FunctionCell> functionCellVec;
    std::vector<std::shared_ptr<DWARFLinker::TypeCell>> typeCellVec;
    std::vector<DWARFLinker::VariableCell> globalVarCellVec;

    // Linker Info

    LinkInfo linkInfo;

    // redundant variables
    std::unordered_set<uint64_t> &redundantVarIdSetRef;
    // <DWARFLoc, <Name, Offset>>
    std::map<uint64_t, std::pair<std::string, uint64_t>> &absVarsInfoRef;


    // constructor
    
    Linker(std::unordered_set<uint64_t> &redundantVarIdSet, std::map<uint64_t, std::pair<std::string, uint64_t>> &absVarsInfo);

    // APIs

    void initSrcInfoObject(const std::shared_ptr<InfoObject> &srcInfoObject);
    void initAbbrevObject();
    void initTypeCells(const std::vector<std::unordered_map<uint64_t, std::shared_ptr<CFG::VTypeInfo>>> &vTypeInfoDataBase);
    void startLink();
    DWARFInfoSpec getSpec();

private:
    void initBaseType();
    std::shared_ptr<TypeCell> recursiveInitType(
        const std::shared_ptr<CFG::VTypeInfo> &vTypeInfo,
        std::unordered_map<std::string, uint64_t> &typeNameSet,
        uint32_t module);
    void recursiveCollect(const InfoEntry &infoEntry);
    void recursiveCollectFuncSpec(const InfoEntry &infoEntry, FunctionCell &functionCell, uint64_t lowPC, uint64_t rangLength);

    void initDstInfoObject();
};

#endif