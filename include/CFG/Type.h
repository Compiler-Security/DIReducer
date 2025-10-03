#ifndef LIBCFG_TYPE_H
#define LIBCFG_TYPE_H

#include <cstdint>               // for uint32_t, uint64_t, uint16_t
#include <memory>                // for shared_ptr
#include <optional>              // for optional
#include <string>                // for string
#include <utility>               // for pair
#include <variant>               // for variant
#include <vector>                // for vector
#include "nlohmann/json_fwd.hpp" // for json

namespace CFG {
    struct VType;
    struct GTypeInfo;
    struct VTypeInfo;
    struct VTypeFactory;
    const int MaxArrayBound     = 512;
    const int DefaultArrayBound = 16;
} // namespace CFG

namespace CFG {
    using VTypePointer = VType *;

    enum class TYPETAG {
        BASE,
        ARRAY,
        POINTER,
        STRUCTURE,
        UNION,
        ENUMERATION,
        FUNCTION,
        VOID
    };

    enum class BASETYPE {
        BT_VOID = 0,
        BT_UINT8,
        BT_UINT16,
        BT_UINT32,
        BT_UINT64,
        BT_UINT128,
        BT_INT8,
        BT_INT16,
        BT_INT32,
        BT_INT64,
        BT_INT128,
        BT_FLOAT32,
        BT_FLOAT64,
        BT_FLOAT128,
        BT_BOOL
    };

    std::string TypeTagToString(TYPETAG type);
    std::string BaseTypeToString(BASETYPE type);
} // namespace CFG

struct CFG::GTypeInfo {
    uint16_t module;
    uint64_t ident;
    uint64_t dwTag;
    std::string name;
    uint32_t size;
    int32_t attr_count;
    uint64_t reference;

    struct {
        struct {
            uint64_t encoding;
        } base;
        struct {
            uint64_t arrayBound;
        } array;
        struct {
            std::vector<std::pair<std::pair<uint64_t, uint32_t>, uint32_t>> memberTypeInfoVec;

        } structType;
        struct {
            // element => type
            std::vector<uint64_t> memberTypeInfoVec;
        } unionType;
    } detail;

    GTypeInfo();

    nlohmann::json to_json(bool withHeader) const;
};

struct CFG::VTypeInfo {
    uint16_t module;
    uint64_t ident;
    uint32_t sn;
    uint64_t dwTag;
    uint64_t referTypeIdent;
    uint64_t referTypeSn;
    uint32_t attr_count;

    std::string name;
    uint32_t size;
    CFG::TYPETAG typeTag;

    // vaild iff typeTag == BASE
    CFG::BASETYPE baseType;
    // valid iff typeTag == ARRAY
    uint64_t arrayBound;
    // valid if typeTag == ARRAY || typeTag == POINTTER || typeTag == FUNCTION
    std::shared_ptr<VTypeInfo> referVTypeInfo;
    // vaild iff typeTag == STRUCTURE, first.first => type, first.second => location, second => bitsize(if bitSize == 0, then it is not a bit member)
    std::vector<std::pair<std::pair<uint64_t, uint32_t>, uint32_t>> structMemberIdentVec;
    // valid iff typeTag == Union
    std::vector<uint64_t> unionMemberIdentVec;
    // valid iff typeTag == STRUCTURE, first.first => type, first.second => location, second => bitsize(if bitSize == 0, then it is not a bit member)
    std::vector<std::pair<std::pair<std::shared_ptr<VTypeInfo>, uint32_t>, uint32_t>> structMemberVTypeInfoVec;
    // valid iff typeTag == Union
    std::vector<std::shared_ptr<VTypeInfo>> unionMemberVTypeInfoVec;

    VTypeInfo();
    bool operator==(const VTypeInfo &rhs) const;
    nlohmann::json to_json() const;
};

struct CFG::VType {
    friend struct CFG::VTypeFactory;

public:
    CFG::TYPETAG typeTag;
    std::string name;
    uint32_t size;
    struct StructMember {
        VTypePointer type;
        uint32_t location;
        StructMember(VTypePointer type, uint32_t location) :
            type(type), location(location) {
        }
    };
    struct ArrayInfo {
        VTypePointer type;
        uint32_t bound;
        ArrayInfo(VTypePointer type, uint32_t bound) :
            type(type), bound(bound) {
        }
    };
    // StructureMemberVec, UnionMemberVec, ArrayInfo, RefType, BaseType
    std::variant<std::vector<StructMember>, std::vector<VTypePointer>, ArrayInfo, VTypePointer, BASETYPE> detail;
    // Pointer Type(optional)
    std::optional<VTypePointer> ptrType;
    // Memory Model, baseType, pointerType
    std::vector<std::pair<uint32_t, VTypePointer>> memoryModel;

private:
    VType(TYPETAG tag, const std::string &name, uint32_t size);
    ~VType() = default;

public:
    VTypePointer getFirstVTypeInMemoryModel(uint32_t typeSize, VTypePointer defaultVType);
    nlohmann::json toJson();

    std::vector<VTypePointer> &getUnionMemberVec();

    std::vector<StructMember> &getStructMemberVec();

    /// @brief Get the Base Type iff typeTag == TYPETAG::BASE
    ///
    /// @return BASETYPE
    BASETYPE getBaseType();

    /// @brief Get the RefType iff typeTag == TYPETAG::POINTER
    ///
    /// @return VTypePointer
    VTypePointer getRefVType();

    /// @brief Get the Array Info iff typeTag == TYPETAG::ARRAY
    ///
    /// @return arrayInfo
    ArrayInfo getArrayInfo();
};

#endif