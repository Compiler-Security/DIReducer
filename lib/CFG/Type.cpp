#include "CFG/Type.h"
#include "Fig/panic.hpp"     // for FIG_PANIC
#include "nlohmann/json.hpp" // for basic_json

namespace CFG {

    std::string TypeTagToString(TYPETAG type) {
        switch(type) {
        case TYPETAG::BASE: return "BASE";
        case TYPETAG::ARRAY: return "ARRAY";
        case TYPETAG::POINTER: return "POINTER";
        case TYPETAG::STRUCTURE: return "STRUCTURE";
        case TYPETAG::ENUMERATION: return "ENUMERATION";
        case TYPETAG::UNION: return "UNION";
        case TYPETAG::FUNCTION: return "FUNCTION";
        case TYPETAG::VOID: return "VOID";
        default: FIG_PANIC("Error TypeTag Enumerator!");
        }
    }

    std::string BaseTypeToString(BASETYPE type) {
        switch(type) {
        case BASETYPE::BT_UINT8: return "BT_UINT8";
        case BASETYPE::BT_UINT16: return "BT_UINT16";
        case BASETYPE::BT_UINT32: return "BT_UINT32";
        case BASETYPE::BT_UINT64: return "BT_UINT64";
        case BASETYPE::BT_UINT128: return "BT_UINT128";
        case BASETYPE::BT_INT8: return "BT_INT8";
        case BASETYPE::BT_INT16: return "BT_INT16";
        case BASETYPE::BT_INT32: return "BT_INT32";
        case BASETYPE::BT_INT64: return "BT_INT64";
        case BASETYPE::BT_INT128: return "BT_INT128";
        case BASETYPE::BT_FLOAT32: return "BT_FLOAT32";
        case BASETYPE::BT_FLOAT64: return "BT_FLOAT64";
        case BASETYPE::BT_FLOAT128: return "BT_FLOAT128";
        case BASETYPE::BT_BOOL: return "BT_BOOL";
        default: FIG_PANIC("Error Base Type Enumerator!");
        }
    }

    GTypeInfo::GTypeInfo() {
        this->module = 0;
        this->name   = "void";
        this->ident  = 0;
        this->dwTag  = 0;
        this->size   = 0;
        this->reference = 0;
    }

    nlohmann::json GTypeInfo::to_json(bool withHeader) const {
        nlohmann::json content = {
            {"module", this->module},
            {"name", this->name},
            {"ident", this->ident},
            {"reference", this->reference},
        };
        if(withHeader)
            return {{"general_type", content}};
        else
            return content;
    }

    VTypeInfo::VTypeInfo() {
        this->module         = 0;
        this->ident          = 0;
        this->sn             = 0;
        this->dwTag          = 0;
        this->referTypeIdent = 0;
        this->referTypeSn    = 0;
        this->name           = "void";
        this->size           = 0;
        this->typeTag        = TYPETAG::VOID;
        this->baseType       = BASETYPE::BT_BOOL;
        this->arrayBound     = 0;
        this->attr_count     = 0;
    }

    bool VTypeInfo::operator==(const VTypeInfo &rhs) const {
        return this->module == rhs.module && this->ident == rhs.ident;
    }

    nlohmann::json VTypeInfo::to_json() const {
        nlohmann::json content = {
            {"module", this->module},
            {"sn", this->sn},
            {"name", this->name},
            {"ident", this->ident},
            {"size", this->size},
            {"tag", CFG::TypeTagToString(this->typeTag)},
        };

        switch(this->typeTag) {
        case TYPETAG::STRUCTURE: {
            content["numOfMember"] = this->structMemberIdentVec.size();
            content["members"]     = nlohmann::json::array();
            for(const auto &member : this->structMemberVTypeInfoVec) {
                content["members"].push_back({{"type", member.first.first->name}, {"location", member.first.second}});
            }
            break;
        }
        case TYPETAG::UNION: {
            content["numOfMember"] = this->unionMemberIdentVec.size();
            content["members"]     = nlohmann::json::array();
            for(const auto &member : this->unionMemberVTypeInfoVec) {
                content["members"].push_back({{"type", member->name}, {"location", 0}});
            }
            break;
        }
        default: break;
        }

        return content;
    }

    VType::VType(TYPETAG tag, const std::string &name, uint32_t size) :
        typeTag(tag), name(name), size(size) {
        switch(tag) {
        case TYPETAG::BASE: this->detail = BASETYPE::BT_INT32; break;
        case TYPETAG::ARRAY: this->detail = VType::ArrayInfo(nullptr, 0); break;
        case TYPETAG::POINTER: this->detail = static_cast<VTypePointer>(nullptr); break;
        case TYPETAG::STRUCTURE: this->detail = std::vector<StructMember>(); break;
        case TYPETAG::UNION: this->detail = std::vector<VTypePointer>(); break;
        case TYPETAG::FUNCTION: break;
        case TYPETAG::VOID: break;
        case TYPETAG::ENUMERATION: break;
        default: FIG_PANIC("unreached path!");
        }
    };

    VTypePointer VType::getFirstVTypeInMemoryModel(uint32_t typeSize, VTypePointer defaultVType) {

        for(auto it : memoryModel) {
            uint32_t location = it.first;
            auto vType        = it.second;
            if(location == 0 && vType->size == typeSize) {
                return vType;
            }
        }
        
        for(auto it : memoryModel) {
            uint32_t location = it.first;
            auto vType        = it.second;
            if(location == 0) {
                return vType;
            }
        }
        // unreached
        return defaultVType;
    }

    nlohmann::json VType::toJson() {
        nlohmann::json content = {
            {"typeTag", TypeTagToString(this->typeTag)},
            {"name", this->name},
            {"size", this->size},
        };
        switch(this->typeTag) {
        case TYPETAG::STRUCTURE: {
            content["StructMembers"] = nlohmann::json::array();
            for(auto &it : this->getStructMemberVec()) {
                content["StructMembers"].push_back(it.type->name);
            }
            break;
        }
        case TYPETAG::UNION: {
            content["UnionMembers"] = nlohmann::json::array();
            for(auto &it : this->getUnionMemberVec()) {
                content["UnionMembers"].push_back(it->name);
            }
            break;
        }
        case TYPETAG::BASE: content["baseType"] = BaseTypeToString(this->getBaseType()); break;
        case TYPETAG::ARRAY: content["elementType"] = this->getArrayInfo().type->name; break;
        case TYPETAG::POINTER: content["refType"] = this->getRefVType()->name; break;
        case TYPETAG::FUNCTION: break;
        case TYPETAG::VOID: break;
        case TYPETAG::ENUMERATION: break;
        default: FIG_PANIC("unreached path!");
        }
        content["memoryModel"] = nlohmann::json::array();
        if(this->memoryModel.size() <= 200) {
            for(auto &it : this->memoryModel) {
                content["memoryModel"].push_back({{"location", it.first}, {"type", it.second->name}});
            }
        } else {
            content["memoryModel"] = "Too Large Memory Model...";
        }

        return content;
    }

    std::vector<VTypePointer> &VType::getUnionMemberVec() {
        if(this->typeTag != TYPETAG::UNION)
            FIG_PANIC("Error Type! type is ", TypeTagToString(this->typeTag), ". It does not have unionMemberVec");

        return std::get<std::vector<VTypePointer>>(this->detail);
    }

    std::vector<VType::StructMember> &VType::getStructMemberVec() {
        if(this->typeTag != TYPETAG::STRUCTURE)
            FIG_PANIC("Error Type! type is ", TypeTagToString(this->typeTag), ". It does not have structMemberVec");

        return std::get<std::vector<StructMember>>(this->detail);
    }

    BASETYPE VType::getBaseType() {
        if(this->typeTag != TYPETAG::BASE)
            FIG_PANIC("Error Type! type is ", TypeTagToString(this->typeTag), ". It is not base type");

        return std::get<BASETYPE>(this->detail);
    }

    VTypePointer VType::getRefVType() {
        if(this->typeTag != TYPETAG::POINTER)
            FIG_PANIC("Error Type! type is ", TypeTagToString(this->typeTag), ". It does not have refType");

        return std::get<VTypePointer>(this->detail);
    }

    VType::ArrayInfo VType::getArrayInfo() {
        if(this->typeTag != TYPETAG::ARRAY)
            FIG_PANIC("Error Type! type is ", TypeTagToString(this->typeTag), ". It does not have refType");

        return std::get<ArrayInfo>(this->detail);
    }
} // namespace CFG
