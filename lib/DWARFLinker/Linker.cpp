#include "DWARFLinker/Linker.h"
#include <cstddef>
#include <cstdint>                    // for uint64_t, uint32_t
#include <cstring>                    // for strcpy
#include <initializer_list>           // for initializer_list
#include <iterator>                   // for next
#include <memory>                     // for shared_ptr, __shared_ptr_access
#include <optional>                   // for optional
#include <string>                     // for hash, operator==, string
#include <unordered_map>              // for unordered_map, _Node_const_ite...
#include <unordered_set>              // for unordered_set
#include <utility>                    // for pair, make_pair
#include <vector>                     // for vector
#include "CFG/Type.h"                 // for VTypeInfo, TYPETAG, BASETYPE
#include "CFG/Utils.h"                // for dec_to_hex
#include "DWARF/AbbrevEntry.h"        // for AbbrevEntry
#include "DWARF/AbbrevItem.h"         // for AbbrevItem, isParameterAbbrev
#include "DWARF/AbbrevList.h"         // for AbbrevList
#include "DWARF/AbbrevObject.h"       // for AbbrevObject
#include "DWARF/AttrBuffer.h"         // for AttrBuffer
#include "DWARF/CompilationUnit.h"    // for CompilationUnit
#include "DWARF/InfoEntry.h"          // for InfoEntry, get_dw_at_type, get...
#include "DWARF/InfoObject.h"         // for InfoObject, DWARFInfoSpec
#include "DWARFLinker/FunctionCell.h" // for FunctionCell
#include "DWARFLinker/TypeCell.h"     // for TypeCell, TypeMember
#include "DWARFLinker/VariableCell.h" // for VariableCell
#include "Fig/assert.hpp"             // for FIG_ASSERT
#include "Fig/panic.hpp"              // for FIG_PANIC
#include "Fig/unreach.hpp"            // for FIG_UNREACH
#include "libdwarf/dwarf.h"           // for DW_TAG_subprogram, DW_AT_type

namespace DWARFLinker {
    Linker::Linker(std::unordered_set<uint64_t> &redundantVarIdSet, std::map<uint64_t, std::pair<std::string, uint64_t>> &absVarsInfo) :
        redundantVarIdSetRef(redundantVarIdSet), absVarsInfoRef(absVarsInfo) {
        this->linkInfo.redundantTypeAttrs = 0;
        this->linkInfo.redundantVarAttrs  = 0;
    }

    void Linker::initSrcInfoObject(const std::shared_ptr<InfoObject> &srcInfoObject) {
        FIG_ASSERT(srcInfoObject.get() != nullptr);
        FIG_ASSERT(srcInfoObject->compilationUnits.size() > 0);

        this->srcInfoObject = srcInfoObject;
        // this->linkInfo.sizeBefore = srcInfoObject->getSize() + srcInfoObject->abbrevObjectRef->getSize();
    }

    void Linker::initAbbrevObject() {
        this->dstAbbrevObject = std::make_shared<AbbrevObject>();
        this->dstAbbrevObject->abbrevlistMap.insert(std::make_pair(0, std::make_shared<AbbrevList>()));
        auto abbrevlist    = this->dstAbbrevObject->getAbbrevList(0);
        abbrevlist->offset = 0;

        AbbrevEntry endEntry{0, 0, 0};
        AbbrevEntry lowPCEntry{DW_AT_low_pc, DW_FORM_data4, 0};
        AbbrevEntry hignPCEntry{DW_AT_high_pc, DW_FORM_data4, 0};
        AbbrevEntry nameEntry{DW_AT_name, DW_FORM_string, 0};
        AbbrevEntry locationEntry{DW_AT_location, DW_FORM_exprloc, 0};
        // AbbrevEntry frameBaseEntry{DW_AT_frame_base, DW_FORM_exprloc, 0};
        AbbrevEntry byteSizeEntry{DW_AT_byte_size, DW_FORM_data2, 0};
        AbbrevEntry encodingEntry{DW_AT_encoding, DW_FORM_data1, 0};
        AbbrevEntry typeEntry{DW_AT_type, DW_FORM_ref4, 0};
        AbbrevEntry upperBoundEntry{DW_AT_upper_bound, DW_FORM_udata, 0};
        AbbrevEntry byteLocEntry{DW_AT_data_member_location, DW_FORM_udata, 0};
        AbbrevEntry bitOffsetEntry{DW_AT_data_bit_offset, DW_FORM_udata, 0};

#define ENTRY_WARP(...) \
    {__VA_ARGS__}
#define ADD_ITEM(ItemName, InAbbrevNumber, TAG, HasChildren, Entries)                           \
    AbbrevItem ItemName{static_cast<uint64_t>(AbbrevNumBer::InAbbrevNumber), TAG, HasChildren}; \
    ItemName.entries = Entries;                                                                 \
    abbrevlist->abbrevItemList.push_back(ItemName);

        ADD_ITEM(item01, Producer, DW_TAG_compile_unit, true, ENTRY_WARP(nameEntry, endEntry))
        ADD_ITEM(item02, FuncWithSubNode, DW_TAG_subprogram, true, ENTRY_WARP(lowPCEntry, hignPCEntry, typeEntry, endEntry))
        ADD_ITEM(item03, FuncWithoutSubNode, DW_TAG_subprogram, false, ENTRY_WARP(lowPCEntry, hignPCEntry, typeEntry, endEntry))
        ADD_ITEM(item04, FuncVoidWithSubNode, DW_TAG_subprogram, true, ENTRY_WARP(lowPCEntry, hignPCEntry, endEntry))
        ADD_ITEM(item05, FuncVoidWithoutSubNode, DW_TAG_subprogram, false, ENTRY_WARP(lowPCEntry, hignPCEntry, endEntry))
        ADD_ITEM(item06, Variable, DW_TAG_variable, false, ENTRY_WARP(locationEntry, typeEntry, endEntry))
        ADD_ITEM(item07, Parameter, DW_TAG_formal_parameter, false, ENTRY_WARP(locationEntry, typeEntry, endEntry))
        ADD_ITEM(item08, BaseType, DW_TAG_base_type, false, ENTRY_WARP(byteSizeEntry, encodingEntry, endEntry))
        ADD_ITEM(item09, PointerType, DW_TAG_pointer_type, false, ENTRY_WARP(typeEntry, endEntry))
        ADD_ITEM(item10, ArrayType, DW_TAG_array_type, false, ENTRY_WARP(typeEntry, upperBoundEntry, endEntry))
        ADD_ITEM(item11, VoidTYPE, DW_TAG_base_type, false, ENTRY_WARP(endEntry))
        ADD_ITEM(item12, StructTYPE, DW_TAG_structure_type, true, ENTRY_WARP(byteSizeEntry, endEntry))
        ADD_ITEM(item13, MemberTYPE, DW_TAG_member, false, ENTRY_WARP(typeEntry, endEntry))
        ADD_ITEM(item14, ByteLocMemberTYPE, DW_TAG_member, false, ENTRY_WARP(byteLocEntry, typeEntry, endEntry))
        ADD_ITEM(item15, BitLocMemberTYPE, DW_TAG_member, false, ENTRY_WARP(bitOffsetEntry, typeEntry, endEntry))
        ADD_ITEM(item16, UnionType, DW_TAG_union_type, true, ENTRY_WARP(byteSizeEntry, endEntry))
        ADD_ITEM(item17, Block, DW_TAG_lexical_block, true, ENTRY_WARP(lowPCEntry, hignPCEntry, endEntry))
        ADD_ITEM(enditem, EndItem, 0, false, ENTRY_WARP())

#undef ADD_ITEM
#undef ENTRY_WARP
    }

    void Linker::initTypeCells(const std::vector<std::unordered_map<uint64_t, std::shared_ptr<CFG::VTypeInfo>>> &vTypeInfoDataBase) {
        this->typeVecMap.clear();
        this->typeVecMap.resize(vTypeInfoDataBase.size());
        // <String, VecIndex>
        std::unordered_map<std::string, uint64_t> typeNameMap;

        this->initBaseType();
        uint32_t module = 1;
        for(const auto &vTypeInfoMap : vTypeInfoDataBase) {
            this->redundantTypes.push_back(std::unordered_set<uint64_t>());
            for(const auto &[loc, vTypeInfo] : vTypeInfoMap) {
                FIG_ASSERT(vTypeInfo.get() != nullptr);
                this->recursiveInitType(vTypeInfo, typeNameMap, module);
            }
            module = module + 1;
        }
    }

    std::shared_ptr<TypeCell> Linker::recursiveInitType(
        const std::shared_ptr<CFG::VTypeInfo> &vTypeInfo,
        std::unordered_map<std::string, uint64_t> &typeNameMap,
        uint32_t module) {
        if(vTypeInfo.get() == nullptr)
            FIG_UNREACH();

        if(typeNameMap.contains(vTypeInfo->name)) {
            // collect redundant type attrs
            if(!this->redundantTypes[module - 1].contains(vTypeInfo->ident)) {
                this->linkInfo.redundantTypeAttrs += vTypeInfo->attr_count;
                this->redundantTypes[module - 1].insert(vTypeInfo->ident);
                // std::cout << vTypeInfo->attr_count << " " << vTypeInfo->name << ", " << vTypeInfo->module << ", " << CFG::dec_to_hex(vTypeInfo->ident) << ", " << this->linkInfo.redundantTypeAttrs << std::endl;
            }

            uint64_t index = typeNameMap[vTypeInfo->name];
            FIG_ASSERT(index < this->typeCellVec.size());
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, index));
            return this->typeCellVec[index];
        }

        if(vTypeInfo->typeTag == CFG::TYPETAG::BASE) {
            // collect redundant type attrs
            if(!this->redundantTypes[module - 1].contains(vTypeInfo->ident)) {
                this->linkInfo.redundantTypeAttrs += vTypeInfo->attr_count;
                this->redundantTypes[module - 1].insert(vTypeInfo->ident);
                // std::cout << vTypeInfo->attr_count << " " << vTypeInfo->name << ", " << vTypeInfo->module << ", " << CFG::dec_to_hex(vTypeInfo->ident) << ", " << this->linkInfo.redundantTypeAttrs << std::endl;
            }

            uint64_t index = static_cast<uint64_t>(vTypeInfo->baseType);
            FIG_ASSERT(index < this->typeCellVec.size());
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, index));
            return this->typeCellVec[index];
        } else if(vTypeInfo->typeTag == CFG::TYPETAG::POINTER) {
            std::shared_ptr<TypeCell> typeCell = std::make_shared<TypeCell>();
            typeCell->name                     = vTypeInfo->name;
            typeCell->tag                      = CFG::TYPETAG::POINTER;
            typeCell->size                     = vTypeInfo->size;
            // void pointer
            if(vTypeInfo->referTypeIdent == 0 || vTypeInfo->referVTypeInfo.get() == nullptr) {
                typeCell->refTypeCell = this->typeCellVec[static_cast<uint64_t>(CFG::BASETYPE::BT_VOID)];
            } else {
                std::shared_ptr<TypeCell> refTypeCell = this->recursiveInitType(vTypeInfo->referVTypeInfo, typeNameMap, module);
                typeCell->refTypeCell                 = refTypeCell;
            }
            typeCell->sn = this->typeCellVec.size();
            this->typeCellVec.push_back(typeCell);
            typeNameMap.insert(std::make_pair(vTypeInfo->name, typeCell->sn));
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, typeCell->sn));
            return this->typeCellVec.back();
        } else if(vTypeInfo->typeTag == CFG::TYPETAG::ARRAY) {
            if(vTypeInfo->referVTypeInfo.get() == nullptr)
                FIG_UNREACH();

            std::shared_ptr<TypeCell> refTypeCell = this->recursiveInitType(vTypeInfo->referVTypeInfo, typeNameMap, module);
            std::shared_ptr<TypeCell> typeCell    = std::make_shared<TypeCell>();
            typeCell->name                        = vTypeInfo->name;
            typeCell->tag                         = CFG::TYPETAG::ARRAY;
            typeCell->size                        = vTypeInfo->size;
            typeCell->refTypeCell                 = refTypeCell;
            typeCell->arrayBound                  = vTypeInfo->arrayBound;
            typeCell->sn                          = this->typeCellVec.size();
            this->typeCellVec.push_back(typeCell);
            typeNameMap.insert(std::make_pair(vTypeInfo->name, typeCell->sn));
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, typeCell->sn));
            return this->typeCellVec.back();
        } else if(vTypeInfo->typeTag == CFG::TYPETAG::ENUMERATION) {
            // => int32

            // collect redundant type attrs
            if(!this->redundantTypes[module - 1].contains(vTypeInfo->ident)) {
                this->linkInfo.redundantTypeAttrs += vTypeInfo->attr_count;
                this->redundantTypes[module - 1].insert(vTypeInfo->ident);
                // std::cout << vTypeInfo->attr_count << " " << vTypeInfo->name << ", " << vTypeInfo->module << ", " << CFG::dec_to_hex(vTypeInfo->ident) << ", " << this->linkInfo.redundantTypeAttrs << std::endl;
            }

            uint64_t index = static_cast<uint64_t>(CFG::BASETYPE::BT_INT32);
            FIG_ASSERT(index < this->typeCellVec.size());
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, index));
            return this->typeCellVec[index];
        } else if(vTypeInfo->typeTag == CFG::TYPETAG::VOID || vTypeInfo->typeTag == CFG::TYPETAG::FUNCTION) {
            uint64_t index = 0;
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, index));
            return this->typeCellVec[index];
        } else if(vTypeInfo->typeTag == CFG::TYPETAG::STRUCTURE) {
            if(vTypeInfo->structMemberVTypeInfoVec.empty()) {
                // collect redundant type attrs
                if(!this->redundantTypes[module - 1].contains(vTypeInfo->ident)) {
                    this->linkInfo.redundantTypeAttrs += vTypeInfo->attr_count;
                    this->redundantTypes[module - 1].insert(vTypeInfo->ident);
                    // std::cout << vTypeInfo->attr_count << " " << vTypeInfo->name << ", " << vTypeInfo->module << ", " << CFG::dec_to_hex(vTypeInfo->ident) << ", " << this->linkInfo.redundantTypeAttrs << std::endl;
                }

                uint64_t index = 0;
                this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, index));
                return this->typeCellVec[index];
            }
            std::shared_ptr<TypeCell> structTypeCell = std::make_shared<TypeCell>();
            structTypeCell->name                     = vTypeInfo->name;
            structTypeCell->tag                      = CFG::TYPETAG::STRUCTURE;
            structTypeCell->size                     = vTypeInfo->size;
            structTypeCell->typeMember               = std::vector<TypeMember>();
            structTypeCell->sn                       = this->typeCellVec.size();
            this->typeCellVec.push_back(structTypeCell);
            typeNameMap.insert(std::make_pair(vTypeInfo->name, structTypeCell->sn));
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, structTypeCell->sn));
            for(const auto &it : vTypeInfo->structMemberVTypeInfoVec) {
                uint32_t byteLocation       = it.first.second;
                uint32_t bitLocation        = it.second;
                const auto &memberVTypeInfo = it.first.first;
                if(memberVTypeInfo.get() == nullptr)
                    FIG_UNREACH();

                std::shared_ptr<TypeCell> memberTypeCell = this->recursiveInitType(memberVTypeInfo, typeNameMap, module);
                TypeMember curMember                     = TypeMember();
                curMember.type                           = memberTypeCell;
                curMember.isBitLocation                  = (bitLocation != 0);
                curMember.memberLocation                 = (bitLocation != 0) ? bitLocation : byteLocation;
                structTypeCell->typeMember->push_back(curMember);
            }
            return structTypeCell;
        } else if(vTypeInfo->typeTag == CFG::TYPETAG::UNION) {
            if(vTypeInfo->unionMemberVTypeInfoVec.empty()) {
                // collect redundant type attrs
                if(!this->redundantTypes[module - 1].contains(vTypeInfo->ident)) {
                    this->linkInfo.redundantTypeAttrs += vTypeInfo->attr_count;
                    this->redundantTypes[module - 1].insert(vTypeInfo->ident);
                    // std::cout << vTypeInfo->attr_count << " " << vTypeInfo->name << ", " << vTypeInfo->module << ", " << CFG::dec_to_hex(vTypeInfo->ident) << ", " << this->linkInfo.redundantTypeAttrs << std::endl;
                }

                uint64_t index = 0;
                this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, index));
                return this->typeCellVec[index];
            }
            std::shared_ptr<TypeCell> unionTypeCell = std::make_shared<TypeCell>();
            unionTypeCell->name                     = vTypeInfo->name;
            unionTypeCell->tag                      = CFG::TYPETAG::UNION;
            unionTypeCell->size                     = vTypeInfo->size;
            unionTypeCell->typeMember               = std::vector<TypeMember>();
            unionTypeCell->sn                       = this->typeCellVec.size();
            this->typeCellVec.push_back(unionTypeCell);
            typeNameMap.insert(std::make_pair(vTypeInfo->name, unionTypeCell->sn));
            this->typeVecMap[module - 1].insert(std::make_pair(vTypeInfo->ident, unionTypeCell->sn));
            for(const auto &memberVTypeInfo : vTypeInfo->unionMemberVTypeInfoVec) {
                if(memberVTypeInfo.get() == nullptr)
                    FIG_UNREACH();
                std::shared_ptr<TypeCell> memberTypeCell = this->recursiveInitType(memberVTypeInfo, typeNameMap, module);
                TypeMember curMember                     = TypeMember();
                curMember.type                           = memberTypeCell;
                curMember.isBitLocation                  = false;
                curMember.memberLocation                 = 0;
                unionTypeCell->typeMember->push_back(curMember);
            }
            return this->typeCellVec.back();
        } else {
            FIG_UNREACH();
        }

        // default void
        return this->typeCellVec[static_cast<uint64_t>(CFG::BASETYPE::BT_VOID)];
    }

    DWARFInfoSpec Linker::getSpec() {
        DWARFInfoSpec spec;
        for(const auto &functionCell : this->functionCellVec) {
            spec.funcAttrsCount += functionCell.getInfoEntry().get_all_attr_count();
            for(const auto &variableCell : functionCell.varCells) {
                spec.varAttrsCount += variableCell->getInfoEntry().get_all_attr_count();
            }
            for(const auto &parameterCell : functionCell.argCells) {
                spec.varAttrsCount += parameterCell->getInfoEntry().get_all_attr_count();
            }
        }
        for(const auto &typeCell : this->typeCellVec) {
            spec.typeAttrsCount += typeCell->getInfoEntry().get_all_attr_count();
        }

        spec.allAttrsCount = spec.funcAttrsCount + spec.typeAttrsCount;
        return spec;
    }

    void Linker::startLink() {
        FIG_ASSERT(this->srcInfoObject.get() != nullptr);
        FIG_ASSERT(this->dstAbbrevObject.get() != nullptr);

        for(const auto &cu : this->srcInfoObject->compilationUnits) {
            const auto &topEntry = cu.infoEntry;
            this->recursiveCollect(topEntry);
        }

        this->initDstInfoObject();

        // this->linkInfo.sizeAfter = this->srcInfoObject->getSize() + this->srcInfoObject->abbrevObjectRef->getSize();
    }

    void Linker::initDstInfoObject() {
        this->dstInfoObject                  = std::make_shared<InfoObject>();
        this->dstInfoObject->abbrevObjectRef = this->dstAbbrevObject;
        this->dstInfoObject->compilationUnits.push_back(CompilationUnit());
        auto &cu              = this->dstInfoObject->compilationUnits[0];
        const auto &exampleCU = this->srcInfoObject->compilationUnits[0];
        // generated by DIRecuder
        auto &topEntry        = cu.infoEntry;
        topEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::Producer);
        topEntry.attrBuffers.push_back(AttrBuffer());
        auto &attrBuffer  = topEntry.attrBuffers.back();
        char slogan[]     = "Produced By DIRecuder.";
        attrBuffer.buffer = std::make_unique<char[]>(sizeof(slogan));
        attrBuffer.size   = sizeof(slogan);
        std::strcpy(attrBuffer.buffer.get(), slogan);

        auto &children    = topEntry.children;
        uint64_t DWARFloc = exampleCU.get_unit_header_size() + topEntry.getSize();
        for(const auto &typeCell : this->typeCellVec) {
            typeCell->DWARFLoc = DWARFloc;
            DWARFloc           = DWARFloc + typeCell->getInfoEntry().getSize();
        }
        for(const auto &typeCell : this->typeCellVec) {
            children.push_back(typeCell->getInfoEntry());
        }
        for(const auto &functionCell : this->functionCellVec) {
            children.push_back(functionCell.getInfoEntry());
        }
        for(const auto &variableCell : this->globalVarCellVec) {
            children.push_back(variableCell.getInfoEntry());
        }
        children.push_back(InfoEntry());
        children.back().abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::EndItem);

        cu.version      = exampleCU.version;
        cu.unit_type    = exampleCU.unit_type;
        cu.addressSize  = exampleCU.addressSize;
        cu.abbrevOffset = 0;
        cu.dwoId        = exampleCU.dwoId;
        cu.typeOffset   = exampleCU.typeOffset;
        cu.bitWidth     = exampleCU.bitWidth;
        cu.length       = cu.getSize() - cu.getLengthSize();
    }

    void Linker::recursiveCollect(const InfoEntry &infoEntry) {
        if(DWARF::isFuncAbbrev(infoEntry.abbrevItem)) {
            FunctionCell functionCell;
            auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
            auto attrRawBufferIter = infoEntry.attrBuffers.begin();
            while(!abbrevEntryIter->isNull()) {
                if(abbrevEntryIter->at == DW_AT_low_pc) {
                    functionCell.address = get_dw_at_low_pc(*abbrevEntryIter, *attrRawBufferIter);
                }
                if(abbrevEntryIter->at == DW_AT_high_pc) {
                    functionCell.size = get_dw_at_high_pc(*abbrevEntryIter, *attrRawBufferIter);
                }
                if(abbrevEntryIter->at == DW_AT_type) {
                    uint64_t typeIndex = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
                    FIG_ASSERT(this->typeVecMap[infoEntry.module - 1].contains(typeIndex), "typeIndex is invalid!");
                    uint64_t typeSN             = this->typeVecMap[infoEntry.module - 1][typeIndex];
                    functionCell.returnTypeCell = this->typeCellVec[typeSN];
                }
                abbrevEntryIter   = std::next(abbrevEntryIter);
                attrRawBufferIter = std::next(attrRawBufferIter);
            }
            for(auto &child : infoEntry.children) {
                this->recursiveCollectFuncSpec(child, functionCell, functionCell.address, functionCell.size);
            }
            this->functionCellVec.push_back(functionCell);

        } else if(DWARF::isVariableAbbrev(infoEntry.abbrevItem)) {
            // global variables
            VariableCell variableCell;
            variableCell.isParameter = false;
            auto abbrevEntryIter     = infoEntry.abbrevItem.entries.begin();
            auto attrRawBufferIter   = infoEntry.attrBuffers.begin();
            uint64_t absOri          = 0;
            while(!abbrevEntryIter->isNull()) {
                uint64_t at   = abbrevEntryIter->at;
                uint64_t form = abbrevEntryIter->form;
                if(at == DW_AT_type) {
                    uint64_t typeIndex = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
                    FIG_ASSERT(this->typeVecMap[infoEntry.module - 1].contains(typeIndex), "typeIndex is invalid!");
                    uint64_t typeSN       = this->typeVecMap[infoEntry.module - 1][typeIndex];
                    variableCell.typeCell = this->typeCellVec[typeSN];
                }
                if(at == DW_AT_abstract_origin || at == DW_AT_specification) {
                    absOri = get_dw_at_abs_ori(*abbrevEntryIter, *attrRawBufferIter) + infoEntry.cuOffset;
                }
                if(at == DW_AT_location) {
                    if(form != DW_FORM_exprloc)
                        FIG_PANIC("Unexpected!");
                    variableCell.location        = *attrRawBufferIter;
                    variableCell.location.offset = 0;
                }

                abbrevEntryIter   = std::next(abbrevEntryIter);
                attrRawBufferIter = std::next(attrRawBufferIter);
            }

            // find abstract origin and fill name&type of variable
            if(absOri != 0) {
                if(!this->absVarsInfoRef.contains(absOri))
                    FIG_PANIC("can not find abstract origin in absVarsInfo. ", "absOri: 0x", CFG::dec_to_hex(absOri));

                auto &absVarInfo   = this->absVarsInfoRef[absOri];
                uint64_t typeIndex = absVarInfo.second;
                FIG_ASSERT(this->typeVecMap[infoEntry.module - 1].contains(typeIndex), "typeIndex is invalid!");
                uint64_t typeSN       = this->typeVecMap[infoEntry.module - 1][typeIndex];
                variableCell.typeCell = this->typeCellVec[typeSN];
            }
            FIG_ASSERT(variableCell.typeCell.get() != nullptr, "typeCell is null!");
            this->globalVarCellVec.push_back(variableCell);
        }

        if(infoEntry.depth == 0) {
            for(auto &child : infoEntry.children) {
                this->recursiveCollect(child);
            }
        }
    }

    void Linker::recursiveCollectFuncSpec(const InfoEntry &infoEntry, FunctionCell &functionCell, uint64_t lowPC, uint64_t rangLength) {
        const auto &item = infoEntry.abbrevItem;
        if(item.tag == DW_TAG_lexical_block) {
            uint64_t lexical_block_low_pc  = lowPC;
            uint64_t lexical_block_high_pc = rangLength;
            auto abbrevEntryIter           = infoEntry.abbrevItem.entries.begin();
            auto attrRawBufferIter         = infoEntry.attrBuffers.begin();
            while(!abbrevEntryIter->isNull()) {
                if(abbrevEntryIter->at == DW_AT_low_pc)
                    lexical_block_low_pc = get_dw_at_low_pc(*abbrevEntryIter, *attrRawBufferIter);
                if(abbrevEntryIter->at == DW_AT_high_pc)
                    lexical_block_high_pc = get_dw_at_high_pc(*abbrevEntryIter, *attrRawBufferIter);

                abbrevEntryIter   = std::next(abbrevEntryIter);
                attrRawBufferIter = std::next(attrRawBufferIter);
            }

            for(auto &child : infoEntry.children) {
                this->recursiveCollectFuncSpec(child, functionCell, lexical_block_low_pc, lexical_block_high_pc);
            }
            return;
        }

        if(DWARF::isVariableAbbrev(item) || DWARF::isParameterAbbrev(item)) {
            uint64_t id = infoEntry.cuOffset + infoEntry.innerOffset;
            // redundancy check
            if(this->redundantVarIdSetRef.contains(id)) {
                this->linkInfo.redundantVarAttrs += infoEntry.get_all_attr_count();
                return;
            }

            auto varCell           = std::make_shared<VariableCell>();
            varCell->isParameter   = DWARF::isParameterAbbrev(item);
            auto abbrevEntryIter   = infoEntry.abbrevItem.entries.begin();
            auto attrRawBufferIter = infoEntry.attrBuffers.begin();
            uint64_t absOri        = 0;
            while(!abbrevEntryIter->isNull()) {
                uint64_t at   = abbrevEntryIter->at;
                uint64_t form = abbrevEntryIter->form;
                if(at == DW_AT_type) {
                    uint64_t typeIndex = get_dw_at_type(*abbrevEntryIter, *attrRawBufferIter);
                    FIG_ASSERT(this->typeVecMap[infoEntry.module - 1].contains(typeIndex), "typeIndex is invalid!");
                    uint64_t typeSN   = this->typeVecMap[infoEntry.module - 1][typeIndex];
                    varCell->typeCell = this->typeCellVec[typeSN];
                }
                if(at == DW_AT_abstract_origin || at == DW_AT_specification) {
                    absOri = get_dw_at_abs_ori(*abbrevEntryIter, *attrRawBufferIter) + infoEntry.cuOffset;
                }
                if(at == DW_AT_location) {
                    if(form != DW_FORM_exprloc)
                        FIG_PANIC("Unexpected!");
                    varCell->location        = *attrRawBufferIter;
                    varCell->location.offset = 0;
                }

                abbrevEntryIter   = std::next(abbrevEntryIter);
                attrRawBufferIter = std::next(attrRawBufferIter);
            }

            // find abstract origin and fill name&type of variable
            if(absOri != 0) {
                if(!this->absVarsInfoRef.contains(absOri))
                    FIG_PANIC("can not find abstract origin in absVarsInfo. ", "absOri: 0x", CFG::dec_to_hex(absOri));

                auto &absVarInfo   = this->absVarsInfoRef[absOri];
                uint64_t typeIndex = absVarInfo.second;
                FIG_ASSERT(this->typeVecMap[infoEntry.module - 1].contains(typeIndex), "typeIndex is invalid!");
                uint64_t typeSN   = this->typeVecMap[infoEntry.module - 1][typeIndex];
                varCell->typeCell = this->typeCellVec[typeSN];
            }
            FIG_ASSERT(varCell->typeCell.get() != nullptr, "typeCell is null!");

            // set variable range
            varCell->lowPC  = lowPC;
            varCell->rangeLength = rangLength;
            if(varCell->isParameter)
                functionCell.argCells.push_back(varCell);
            else
                functionCell.varCells.push_back(varCell);
        }
    }

    void Linker::initBaseType() {
        std::shared_ptr<TypeCell> voidTypeCell     = std::make_shared<TypeCell>(CFG::TYPETAG::VOID);
        voidTypeCell->sn                           = 0;
        voidTypeCell->name                         = "void";
        voidTypeCell->basetype                     = CFG::BASETYPE::BT_VOID;
        std::shared_ptr<TypeCell> uint8TypeCell    = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        uint8TypeCell->sn                          = 1;
        uint8TypeCell->name                        = "uint8";
        uint8TypeCell->size                        = 1;
        uint8TypeCell->basetype                    = CFG::BASETYPE::BT_UINT8;
        std::shared_ptr<TypeCell> uint16TypeCell   = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        uint16TypeCell->sn                         = 2;
        uint16TypeCell->name                       = "uint16";
        uint16TypeCell->size                       = 2;
        uint16TypeCell->basetype                   = CFG::BASETYPE::BT_UINT16;
        std::shared_ptr<TypeCell> uint32TypeCell   = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        uint32TypeCell->sn                         = 3;
        uint32TypeCell->name                       = "uint32";
        uint32TypeCell->size                       = 4;
        uint32TypeCell->basetype                   = CFG::BASETYPE::BT_UINT32;
        std::shared_ptr<TypeCell> uint64TypeCell   = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        uint64TypeCell->sn                         = 4;
        uint64TypeCell->name                       = "uint64";
        uint64TypeCell->size                       = 8;
        uint64TypeCell->basetype                   = CFG::BASETYPE::BT_UINT64;
        std::shared_ptr<TypeCell> uint128TypeCell  = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        uint128TypeCell->sn                        = 5;
        uint128TypeCell->name                      = "uint128";
        uint128TypeCell->size                      = 16;
        uint128TypeCell->basetype                  = CFG::BASETYPE::BT_UINT128;
        std::shared_ptr<TypeCell> int8TypeCell     = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        int8TypeCell->sn                           = 6;
        int8TypeCell->name                         = "int8";
        int8TypeCell->size                         = 1;
        int8TypeCell->basetype                     = CFG::BASETYPE::BT_INT8;
        std::shared_ptr<TypeCell> int16TypeCell    = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        int16TypeCell->sn                          = 7;
        int16TypeCell->name                        = "int16";
        int16TypeCell->size                        = 2;
        int16TypeCell->basetype                    = CFG::BASETYPE::BT_INT16;
        std::shared_ptr<TypeCell> int32TypeCell    = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        int32TypeCell->sn                          = 8;
        int32TypeCell->name                        = "int32";
        int32TypeCell->size                        = 4;
        int32TypeCell->basetype                    = CFG::BASETYPE::BT_INT32;
        std::shared_ptr<TypeCell> int64TypeCell    = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        int64TypeCell->sn                          = 9;
        int64TypeCell->name                        = "int64";
        int64TypeCell->size                        = 8;
        int64TypeCell->basetype                    = CFG::BASETYPE::BT_INT64;
        std::shared_ptr<TypeCell> int128TypeCell   = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        int128TypeCell->sn                         = 10;
        int128TypeCell->name                       = "int128";
        int128TypeCell->size                       = 16;
        int128TypeCell->basetype                   = CFG::BASETYPE::BT_INT128;
        std::shared_ptr<TypeCell> float32TypeCell  = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        float32TypeCell->sn                        = 11;
        float32TypeCell->name                      = "float32";
        float32TypeCell->size                      = 4;
        float32TypeCell->basetype                  = CFG::BASETYPE::BT_FLOAT32;
        std::shared_ptr<TypeCell> float64TypeCell  = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        float64TypeCell->sn                        = 12;
        float64TypeCell->name                      = "float64";
        float64TypeCell->size                      = 8;
        float64TypeCell->basetype                  = CFG::BASETYPE::BT_FLOAT64;
        std::shared_ptr<TypeCell> float128TypeCell = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        float128TypeCell->sn                       = 13;
        float128TypeCell->name                     = "float128";
        float128TypeCell->size                     = 16;
        float128TypeCell->basetype                 = CFG::BASETYPE::BT_FLOAT128;
        std::shared_ptr<TypeCell> boolTypeCell     = std::make_shared<TypeCell>(CFG::TYPETAG::BASE);
        boolTypeCell->sn                           = 14;
        boolTypeCell->name                         = "bool";
        boolTypeCell->size                         = 1;
        boolTypeCell->basetype                     = CFG::BASETYPE::BT_BOOL;

        this->typeCellVec.push_back(voidTypeCell);
        this->typeCellVec.push_back(uint8TypeCell);
        this->typeCellVec.push_back(uint16TypeCell);
        this->typeCellVec.push_back(uint32TypeCell);
        this->typeCellVec.push_back(uint64TypeCell);
        this->typeCellVec.push_back(uint128TypeCell);
        this->typeCellVec.push_back(int8TypeCell);
        this->typeCellVec.push_back(int16TypeCell);
        this->typeCellVec.push_back(int32TypeCell);
        this->typeCellVec.push_back(int64TypeCell);
        this->typeCellVec.push_back(int128TypeCell);
        this->typeCellVec.push_back(float32TypeCell);
        this->typeCellVec.push_back(float64TypeCell);
        this->typeCellVec.push_back(float128TypeCell);
        this->typeCellVec.push_back(boolTypeCell);
    }
} // namespace DWARFLinker