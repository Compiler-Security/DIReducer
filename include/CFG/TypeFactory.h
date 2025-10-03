#ifndef LIBCFG_TYPEFACTORY_H
#define LIBCFG_TYPEFACTORY_H

#include <nlohmann/json.hpp>
#include "Type.h"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CFG {
    struct VTypeFactory;
}

struct CFG::VTypeFactory {
private:
    static std::unordered_set<VTypePointer> vTypeSet;
    static std::vector<std::unordered_map<uint64_t, VTypePointer>> moduleOffsetMapVec;

private:
    struct internalBaseTypeCollection {
        VTypePointer bt_uint8;
        VTypePointer bt_uint16;
        VTypePointer bt_uint32;
        VTypePointer bt_uint64;
        VTypePointer bt_uint128;
        VTypePointer bt_int8;
        VTypePointer bt_int16;
        VTypePointer bt_int32;
        VTypePointer bt_int64;
        VTypePointer bt_int128;
        VTypePointer bt_float32;
        VTypePointer bt_float64;
        VTypePointer bt_float128;
        VTypePointer bt_bool;
        VTypePointer bt_void;
    };

public:
    static internalBaseTypeCollection baseVType;

public:
    static nlohmann::json getVTypeJson();
    static void init(uint32_t moduleSize);
    static bool isBoolType(VTypePointer vType);
    static bool isArrayType(VTypePointer vType);
    static bool isFloatType(VTypePointer vType);
    static bool isIntegerType(VTypePointer vType);
    static bool isPointerType(VTypePointer vType);
    static bool isSignedIntType(VTypePointer vType);
    static bool isUnsignedIntType(VTypePointer vType);
    static const std::unordered_set<VTypePointer> &getVTYpeSet();
    static bool isVTypeInfoExist(uint16_t module, uint64_t offset);
    static VTypePointer getIntegerType(bool isSigned, uint8_t byteSize);
    static VTypePointer getVTypeByVTypeInfo(const VTypeInfo &vTypeInfo);
    static VTypePointer getVTypeByModuleOffset(uint16_t module, uint64_t offset);
    static const std::vector<std::unordered_map<uint64_t, VTypePointer>> &getModuleOffsetMapVec();

    // global
    static VTypePointer getPointerType(VTypePointer vType);
    static VTypePointer getArrayType(VTypePointer vType, uint64_t bound);

    // local
    static void destroyCustomType(VTypePointer vType);
    static VTypePointer getCustomType(TYPETAG tag, const std::string &name, uint32_t size);
};

#endif