#include <cstdint>               // for uint32_t, uint64_t, uint16_t, uint8_t
#include <initializer_list>      // for initializer_list
#include <nlohmann/json.hpp>     // for basic_json
#include <optional>              // for optional
#include <string>                // for char_traits, operator+, basic_string
#include <unordered_map>         // for unordered_map, operator==, _Node_it...
#include <unordered_set>         // for unordered_set
#include <utility>               // for pair, make_pair
#include <variant>               // for variant
#include <vector>                // for vector
#include "CFG/Type.h"            // for VTypePointer, VType, VTypeInfo, TYP...
#include "CFG/TypeFactory.h"     // for VTypeFactory
#include "CFG/Utils.h"           // for dec_to_hex
#include "Fig/panic.hpp"         // for FIG_PANIC
#include "nlohmann/json_fwd.hpp" // for json

namespace CFG {
    std::unordered_set<VTypePointer> VTypeFactory::vTypeSet                                  = std::unordered_set<VTypePointer>();
    std::vector<std::unordered_map<uint64_t, VTypePointer>> VTypeFactory::moduleOffsetMapVec = std::vector<std::unordered_map<uint64_t, VTypePointer>>();
    VTypeFactory::internalBaseTypeCollection VTypeFactory::baseVType                         = {
                                .bt_uint8    = static_cast<VTypePointer>(nullptr),
                                .bt_uint16   = static_cast<VTypePointer>(nullptr),
                                .bt_uint32   = static_cast<VTypePointer>(nullptr),
                                .bt_uint64   = static_cast<VTypePointer>(nullptr),
                                .bt_uint128  = static_cast<VTypePointer>(nullptr),
                                .bt_int8     = static_cast<VTypePointer>(nullptr),
                                .bt_int16    = static_cast<VTypePointer>(nullptr),
                                .bt_int32    = static_cast<VTypePointer>(nullptr),
                                .bt_int64    = static_cast<VTypePointer>(nullptr),
                                .bt_int128   = static_cast<VTypePointer>(nullptr),
                                .bt_float32  = static_cast<VTypePointer>(nullptr),
                                .bt_float64  = static_cast<VTypePointer>(nullptr),
                                .bt_float128 = static_cast<VTypePointer>(nullptr),
                                .bt_bool     = static_cast<VTypePointer>(nullptr),
                                .bt_void     = static_cast<VTypePointer>(nullptr)};

    void VTypeFactory::init(uint32_t moduleSize) {
        VTypeFactory::vTypeSet.clear();
        VTypeFactory::moduleOffsetMapVec.clear();
        VTypeFactory::moduleOffsetMapVec.resize(moduleSize);

        VTypeFactory::baseVType.bt_uint8    = new VType(TYPETAG::BASE, "uint8", 1);
        VTypeFactory::baseVType.bt_uint16   = new VType(TYPETAG::BASE, "uint16", 2);
        VTypeFactory::baseVType.bt_uint32   = new VType(TYPETAG::BASE, "uint32", 4);
        VTypeFactory::baseVType.bt_uint64   = new VType(TYPETAG::BASE, "uint64", 8);
        VTypeFactory::baseVType.bt_uint128  = new VType(TYPETAG::BASE, "uint128", 16);
        VTypeFactory::baseVType.bt_int8     = new VType(TYPETAG::BASE, "int8", 1);
        VTypeFactory::baseVType.bt_int16    = new VType(TYPETAG::BASE, "int16", 2);
        VTypeFactory::baseVType.bt_int32    = new VType(TYPETAG::BASE, "int32", 4);
        VTypeFactory::baseVType.bt_int64    = new VType(TYPETAG::BASE, "int64", 8);
        VTypeFactory::baseVType.bt_int128   = new VType(TYPETAG::BASE, "int128", 16);
        VTypeFactory::baseVType.bt_float32  = new VType(TYPETAG::BASE, "float32", 4);
        VTypeFactory::baseVType.bt_float64  = new VType(TYPETAG::BASE, "float64", 8);
        VTypeFactory::baseVType.bt_float128 = new VType(TYPETAG::BASE, "float128", 16);
        VTypeFactory::baseVType.bt_bool     = new VType(TYPETAG::BASE, "bool", 1);
        VTypeFactory::baseVType.bt_void     = new VType(TYPETAG::VOID, "void", 0);

        VTypeFactory::baseVType.bt_uint8->detail    = BASETYPE::BT_UINT8;
        VTypeFactory::baseVType.bt_uint16->detail   = BASETYPE::BT_UINT16;
        VTypeFactory::baseVType.bt_uint32->detail   = BASETYPE::BT_UINT32;
        VTypeFactory::baseVType.bt_uint64->detail   = BASETYPE::BT_UINT64;
        VTypeFactory::baseVType.bt_uint128->detail  = BASETYPE::BT_UINT128;
        VTypeFactory::baseVType.bt_int8->detail     = BASETYPE::BT_INT8;
        VTypeFactory::baseVType.bt_int16->detail    = BASETYPE::BT_INT16;
        VTypeFactory::baseVType.bt_int32->detail    = BASETYPE::BT_INT32;
        VTypeFactory::baseVType.bt_int64->detail    = BASETYPE::BT_INT64;
        VTypeFactory::baseVType.bt_int128->detail   = BASETYPE::BT_INT128;
        VTypeFactory::baseVType.bt_float32->detail  = BASETYPE::BT_FLOAT32;
        VTypeFactory::baseVType.bt_float64->detail  = BASETYPE::BT_FLOAT64;
        VTypeFactory::baseVType.bt_float128->detail = BASETYPE::BT_FLOAT128;
        VTypeFactory::baseVType.bt_bool->detail     = BASETYPE::BT_BOOL;

        VTypeFactory::baseVType.bt_uint8->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_uint8));
        VTypeFactory::baseVType.bt_uint16->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_uint16));
        VTypeFactory::baseVType.bt_uint32->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_uint32));
        VTypeFactory::baseVType.bt_uint64->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_uint64));
        VTypeFactory::baseVType.bt_uint128->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_uint64));
        VTypeFactory::baseVType.bt_uint128->memoryModel.push_back(std::make_pair(8, VTypeFactory::baseVType.bt_uint64));
        VTypeFactory::baseVType.bt_int8->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_int8));
        VTypeFactory::baseVType.bt_int16->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_int16));
        VTypeFactory::baseVType.bt_int32->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_int32));
        VTypeFactory::baseVType.bt_int64->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_int64));
        VTypeFactory::baseVType.bt_int128->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_int64));
        VTypeFactory::baseVType.bt_int128->memoryModel.push_back(std::make_pair(8, VTypeFactory::baseVType.bt_int64));
        VTypeFactory::baseVType.bt_float32->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_float32));
        VTypeFactory::baseVType.bt_float64->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_float64));
        VTypeFactory::baseVType.bt_float128->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_float128));
        VTypeFactory::baseVType.bt_bool->memoryModel.push_back(std::make_pair(0, VTypeFactory::baseVType.bt_bool));

        vTypeSet.insert(VTypeFactory::baseVType.bt_uint8);
        vTypeSet.insert(VTypeFactory::baseVType.bt_uint16);
        vTypeSet.insert(VTypeFactory::baseVType.bt_uint32);
        vTypeSet.insert(VTypeFactory::baseVType.bt_uint64);
        vTypeSet.insert(VTypeFactory::baseVType.bt_uint128);
        vTypeSet.insert(VTypeFactory::baseVType.bt_int8);
        vTypeSet.insert(VTypeFactory::baseVType.bt_int16);
        vTypeSet.insert(VTypeFactory::baseVType.bt_int32);
        vTypeSet.insert(VTypeFactory::baseVType.bt_int64);
        vTypeSet.insert(VTypeFactory::baseVType.bt_int128);
        vTypeSet.insert(VTypeFactory::baseVType.bt_float32);
        vTypeSet.insert(VTypeFactory::baseVType.bt_float64);
        vTypeSet.insert(VTypeFactory::baseVType.bt_float128);
        vTypeSet.insert(VTypeFactory::baseVType.bt_bool);
        vTypeSet.insert(VTypeFactory::baseVType.bt_void);
    }

    VTypePointer VTypeFactory::getVTypeByVTypeInfo(const VTypeInfo &vTypeInfo) {
        // check module number
        if(vTypeInfo.module > VTypeFactory::moduleOffsetMapVec.size())
            FIG_PANIC("vTypeInfo.module > VTypeFactory::moduleOffsetMapVec.size()");

        if(VTypeFactory::isVTypeInfoExist(vTypeInfo.module, vTypeInfo.ident))
            return VTypeFactory::getVTypeByModuleOffset(vTypeInfo.module, vTypeInfo.ident);

        switch(vTypeInfo.typeTag) {
        case TYPETAG::BASE: {
            VTypePointer targetBaseType;
            switch(vTypeInfo.baseType) {
            case BASETYPE::BT_UINT8: targetBaseType = VTypeFactory::baseVType.bt_uint8; break;
            case BASETYPE::BT_UINT16: targetBaseType = VTypeFactory::baseVType.bt_uint16; break;
            case BASETYPE::BT_UINT32: targetBaseType = VTypeFactory::baseVType.bt_uint32; break;
            case BASETYPE::BT_UINT64: targetBaseType = VTypeFactory::baseVType.bt_uint64; break;
            case BASETYPE::BT_UINT128: targetBaseType = VTypeFactory::baseVType.bt_uint128; break;
            case BASETYPE::BT_INT8: targetBaseType = VTypeFactory::baseVType.bt_int8; break;
            case BASETYPE::BT_INT16: targetBaseType = VTypeFactory::baseVType.bt_int16; break;
            case BASETYPE::BT_INT32: targetBaseType = VTypeFactory::baseVType.bt_int32; break;
            case BASETYPE::BT_INT64: targetBaseType = VTypeFactory::baseVType.bt_int64; break;
            case BASETYPE::BT_INT128: targetBaseType = VTypeFactory::baseVType.bt_int128; break;
            case BASETYPE::BT_FLOAT32: targetBaseType = VTypeFactory::baseVType.bt_float32; break;
            case BASETYPE::BT_FLOAT64: targetBaseType = VTypeFactory::baseVType.bt_float64; break;
            case BASETYPE::BT_FLOAT128: targetBaseType = VTypeFactory::baseVType.bt_float128; break;
            case BASETYPE::BT_BOOL: targetBaseType = VTypeFactory::baseVType.bt_bool; break;
            default: FIG_PANIC("unreached path! ", "baseType:", static_cast<uint32_t>(vTypeInfo.baseType));
            }
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = targetBaseType;
            return targetBaseType;
        }
        case TYPETAG::ARRAY: {
            VTypePointer newArrayType = new VType(TYPETAG::ARRAY, vTypeInfo.name, vTypeInfo.size);
            VTypeFactory::vTypeSet.insert(newArrayType);
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = newArrayType;
            if(vTypeInfo.referVTypeInfo) {
                auto &referVTypeInfo = vTypeInfo.referVTypeInfo;
                VTypePointer refType = VTypeFactory::getVTypeByVTypeInfo(*referVTypeInfo);
                if(vTypeInfo.arrayBound > 0)
                    newArrayType->name = refType->name + "[" + std::to_string(vTypeInfo.arrayBound) + "]";
                else
                    newArrayType->name = refType->name + "[]";
                newArrayType->detail = VType::ArrayInfo(refType, vTypeInfo.arrayBound);
                // memorymodel
                uint32_t baseLocation = 0;
                for(uint32_t i = 0; i < vTypeInfo.arrayBound; i++) {
                    for(auto &it : refType->memoryModel) {
                        newArrayType->memoryModel.push_back(std::make_pair(baseLocation + it.first, it.second));
                    }
                    baseLocation = baseLocation + refType->size;
                }
            } else {
                FIG_PANIC("array type has not sub type!");
            }
            return newArrayType;
        }
        case TYPETAG::POINTER: {
            VTypePointer newPointerType = new VType(TYPETAG::POINTER, vTypeInfo.name, vTypeInfo.size);
            VTypeFactory::vTypeSet.insert(newPointerType);
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = newPointerType;
            if(vTypeInfo.referVTypeInfo) {
                auto &referVTypeInfo   = vTypeInfo.referVTypeInfo;
                VTypePointer refType   = VTypeFactory::getVTypeByVTypeInfo(*referVTypeInfo);
                newPointerType->name   = refType->name + "*";
                newPointerType->detail = refType;
                refType->ptrType.emplace(newPointerType);
            } else {
                newPointerType->name   = "void*";
                newPointerType->detail = VTypeFactory::baseVType.bt_void;
            }
            // memorymodel
            newPointerType->memoryModel.push_back(std::make_pair(0, newPointerType));
            return newPointerType;
        }
        case TYPETAG::UNION: {
            VTypePointer newUnionType = new VType(TYPETAG::UNION, vTypeInfo.name, vTypeInfo.size);
            VTypeFactory::vTypeSet.insert(newUnionType);
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = newUnionType;
            auto &unionMemberVec                                                    = newUnionType->getUnionMemberVec();
            for(const auto &it : vTypeInfo.unionMemberVTypeInfoVec) {
                VTypePointer memberType = VTypeFactory::getVTypeByVTypeInfo(*it);
                unionMemberVec.push_back(memberType);
            }
            // memorymodel
            for(auto &memberVType : unionMemberVec) {
                for(auto &it : memberVType->memoryModel) {
                    uint32_t curLocation = it.first;
                    newUnionType->memoryModel.push_back(std::make_pair(curLocation, it.second));
                }
            }
            return newUnionType;
        }
        case TYPETAG::STRUCTURE: {
            VTypePointer newStructType = new VType(TYPETAG::STRUCTURE, vTypeInfo.name, vTypeInfo.size);
            VTypeFactory::vTypeSet.insert(newStructType);
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = newStructType;
            auto &structMemberVec                                                   = newStructType->getStructMemberVec();
            for(const auto &it : vTypeInfo.structMemberVTypeInfoVec) {
                auto &memberVTypeInfo   = it.first.first;
                uint32_t location       = it.first.second;
                VTypePointer memberType = VTypeFactory::getVTypeByVTypeInfo(*memberVTypeInfo);
                uint32_t bitSize = it.second;
                if(bitSize != 0) {
                    if(!VTypeFactory::isIntegerType(memberType)) FIG_PANIC("Bit Member's type is not integer type! Type name: ", memberType->name);
                    uint32_t bitOffset = it.first.second;
                    uint32_t byteSize  = 0;
                    for(auto it : {1, 2, 4, 8}) {
                        uint32_t byteLocation = bitOffset / 8;
                        uint32_t excessByte   = byteLocation % it;
                        byteLocation          = byteLocation - excessByte;
                        if(byteLocation + it >= ((bitOffset + bitSize) / 8)) {
                            location = byteLocation;
                            byteSize = it;
                            break;
                        }
                    }
                    if(byteSize == 0) FIG_PANIC("unsupported! bitoffset:", bitOffset, " bitsize:", bitSize);
                    bool isSigned = VTypeFactory::isSignedIntType(memberType);
                    memberType    = VTypeFactory::getIntegerType(isSigned, byteSize);
                }
                structMemberVec.emplace_back(memberType, location);
            }
            // memorymodel
            for(auto &memberVTypeEntry : structMemberVec) {
                VTypePointer memberVType = memberVTypeEntry.type;
                uint32_t baseLocation    = memberVTypeEntry.location;
                for(auto &it : memberVType->memoryModel) {
                    uint32_t curLocation = baseLocation + it.first;
                    newStructType->memoryModel.push_back(std::make_pair(curLocation, it.second));
                }
            }
            return newStructType;
        }
        case TYPETAG::ENUMERATION: {
            VTypePointer enumType;
            switch(vTypeInfo.size) {
            case 1: enumType = VTypeFactory::baseVType.bt_int8; break;
            case 2: enumType = VTypeFactory::baseVType.bt_int16; break;
            case 4: enumType = VTypeFactory::baseVType.bt_int32; break;
            case 8: enumType = VTypeFactory::baseVType.bt_int64; break;
            default: FIG_PANIC("unsupported enum size : ", vTypeInfo.size);
            }
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = enumType;
            return enumType;
        }
        case TYPETAG::FUNCTION: {
            VTypePointer newFuncType = new VType(TYPETAG::FUNCTION, vTypeInfo.name, vTypeInfo.size);
            VTypeFactory::vTypeSet.insert(newFuncType);
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = newFuncType;
            if(vTypeInfo.referVTypeInfo) {
                auto &referVTypeInfo = vTypeInfo.referVTypeInfo;
                VTypePointer refType = VTypeFactory::getVTypeByVTypeInfo(*referVTypeInfo);
                newFuncType->name    = "(" + refType->name + "*)";
                newFuncType->detail  = refType;
            } else {
                newFuncType->name   = "(void*)";
                newFuncType->detail = VTypeFactory::baseVType.bt_void;
            }
            // memorymodel
            return newFuncType;
        }
        case TYPETAG::VOID: {
            VTypePointer voidType                                                   = baseVType.bt_void;
            VTypeFactory::moduleOffsetMapVec[vTypeInfo.module - 1][vTypeInfo.ident] = voidType;
            return voidType;
        }
        default: FIG_PANIC("unsupported TYPETAG : ", static_cast<uint32_t>(vTypeInfo.typeTag));
        }
    }

    bool VTypeFactory::isVTypeInfoExist(uint16_t module, uint64_t offset) {
        if(module > VTypeFactory::moduleOffsetMapVec.size())
            FIG_PANIC("module > TypeFactory::moduleOffsetMap.size()");

        auto &offsetMap = VTypeFactory::moduleOffsetMapVec[module - 1];
        if(offsetMap.find(offset) == offsetMap.end())
            return false;
        else
            return true;
    }

    VTypePointer VTypeFactory::getVTypeByModuleOffset(uint16_t module, uint64_t offset) {
        if(module > VTypeFactory::moduleOffsetMapVec.size())
            FIG_PANIC("module > TypeFactory::moduleOffsetMap.size()");

        auto &offsetMap = VTypeFactory::moduleOffsetMapVec[module - 1];
        if(offsetMap.find(offset) == offsetMap.end())
            FIG_PANIC("Can not Find Type in offsetMap. Type offset: 0x", dec_to_hex(offset));

        return offsetMap[offset];
    }

    VTypePointer VTypeFactory::getPointerType(VTypePointer vType) {
        if(vType->ptrType.has_value())
            return vType->ptrType.value();

        VTypePointer newPointerType = new VType(TYPETAG::POINTER, vType->name + "*", 8);
        VTypeFactory::vTypeSet.insert(newPointerType);
        newPointerType->detail = vType;
        vType->ptrType.emplace(newPointerType);
        // memoryModel
        newPointerType->memoryModel.push_back(std::make_pair(0, newPointerType));
        return newPointerType;
    }

    VTypePointer VTypeFactory::getArrayType(VTypePointer vType, uint64_t bound) {
        VTypePointer newArrayType = new VType(TYPETAG::ARRAY, vType->name + "[" + std::to_string(bound) + "]", vType->size * bound);
        VTypeFactory::vTypeSet.insert(newArrayType);
        newArrayType->detail = VType::ArrayInfo(vType, bound);
        // memoryModel
        uint32_t baseLocation = 0;
        for(uint32_t i = 0; i < bound; i++) {
            for(auto &it : vType->memoryModel) {
                newArrayType->memoryModel.push_back(std::make_pair(baseLocation + it.first, it.second));
            }
            baseLocation = baseLocation + vType->size;
        }
        return newArrayType;
    }

    VTypePointer VTypeFactory::getCustomType(TYPETAG tag, const std::string &name, uint32_t size) {
        return new VType(tag, name, size);
    }

    void VTypeFactory::destroyCustomType(VTypePointer vType) {
        delete vType;
    }

    const std::unordered_set<VTypePointer> &VTypeFactory::getVTYpeSet() {
        return VTypeFactory::vTypeSet;
    }

    const std::vector<std::unordered_map<uint64_t, VTypePointer>> &VTypeFactory::getModuleOffsetMapVec() {
        return VTypeFactory::moduleOffsetMapVec;
    }

    VTypePointer VTypeFactory::getIntegerType(bool isSigned, uint8_t byteSize) {
        if(isSigned) {
            switch(byteSize) {
            case 1: return VTypeFactory::baseVType.bt_int8;
            case 2: return VTypeFactory::baseVType.bt_int16;
            case 4: return VTypeFactory::baseVType.bt_int32;
            case 8: return VTypeFactory::baseVType.bt_int64;
            case 16: return VTypeFactory::baseVType.bt_int128;
            default: FIG_PANIC("unsupported bytesize! bytesize:", static_cast<uint16_t>(byteSize));
            }
        } else {
            switch(byteSize) {
            case 1: return VTypeFactory::baseVType.bt_uint8;
            case 2: return VTypeFactory::baseVType.bt_uint16;
            case 4: return VTypeFactory::baseVType.bt_uint32;
            case 8: return VTypeFactory::baseVType.bt_uint64;
            case 16: return VTypeFactory::baseVType.bt_uint128;
            default: FIG_PANIC("unsupported bytesize! bytesize:", static_cast<uint16_t>(byteSize));
            }
        }
    }

    bool VTypeFactory::isUnsignedIntType(VTypePointer vType) {
        if(vType == VTypeFactory::baseVType.bt_uint8 || vType == VTypeFactory::baseVType.bt_uint16 || vType == VTypeFactory::baseVType.bt_uint32
           || vType == VTypeFactory::baseVType.bt_uint64 || vType == VTypeFactory::baseVType.bt_uint128) {
            return true;
        } else {
            return false;
        }
    }

    bool VTypeFactory::isSignedIntType(VTypePointer vType) {
        if(vType == VTypeFactory::baseVType.bt_int8 || vType == VTypeFactory::baseVType.bt_int16 || vType == VTypeFactory::baseVType.bt_int32
           || vType == VTypeFactory::baseVType.bt_int64 || vType == VTypeFactory::baseVType.bt_int128) {
            return true;
        } else {
            return false;
        }
    }

    bool VTypeFactory::isBoolType(VTypePointer vType) {
        return vType == VTypeFactory::baseVType.bt_bool;
    }

    bool isArrayType(VTypePointer vType) {
        return vType->typeTag == TYPETAG::ARRAY;
    }

    bool VTypeFactory::isFloatType(VTypePointer vType) {
        if(vType == VTypeFactory::baseVType.bt_float32 || vType == VTypeFactory::baseVType.bt_float64 || vType == VTypeFactory::baseVType.bt_float128) {
            return true;
        } else {
            return false;
        }
    }

    bool VTypeFactory::isIntegerType(VTypePointer vType) {
        return VTypeFactory::isSignedIntType(vType) || VTypeFactory::isUnsignedIntType(vType);
    }

    bool VTypeFactory::isPointerType(VTypePointer vType) {
        return vType->typeTag == TYPETAG::POINTER;
    }

    nlohmann::json VTypeFactory::getVTypeJson() {
        nlohmann::json content;
        nlohmann::json vTypes = nlohmann::json::array();
        for(auto &it : VTypeFactory::vTypeSet) {
            vTypes.push_back(it->toJson());
        }

        content["numOfVTypes"] = VTypeFactory::vTypeSet.size();
        content["vTypes"]      = vTypes;
        return content;
    }

} // namespace CFG