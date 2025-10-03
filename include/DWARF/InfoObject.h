#ifndef INFO_OBJECT_H
#define INFO_OBJECT_H

#include <cstdint>
#include <memory>
#include <vector>

#include "DWARFBuffer.h"
#include "CompilationUnit.h"
#include "AbbrevObject.h"
#include "LoclistsObject.h"
#include "nlohmann/json_fwd.hpp"

struct DWARFInfoSpec {
    uint64_t allAttrsCount;
    uint64_t typeAttrsCount;
    uint64_t funcAttrsCount;
    uint64_t varAttrsCount;
    uint64_t othersAttrsCount;
    DWARFInfoSpec() :
        allAttrsCount(0), typeAttrsCount(0), funcAttrsCount(0), varAttrsCount(0), othersAttrsCount(0) {
    }

    nlohmann::json toJson() {
        nlohmann::json content = {
            {"allAttrsCount", this->allAttrsCount},
            {"typeAttrsCount", this->typeAttrsCount},
            {"funcAttrsCount", this->funcAttrsCount},
            {"varAttrsCount", this->varAttrsCount},
            {"othersAttrsCount", this->othersAttrsCount},
        };

        return content;
    }
};

struct InfoObject {
public:
    std::shared_ptr<AbbrevObject> abbrevObjectRef;
    std::vector<CompilationUnit> compilationUnits;

public:
    InfoObject() = default;
    InfoObject(DWARFBuffer &dwarfBuffer, std::shared_ptr<AbbrevObject> abbrevObject);

    ~InfoObject() = default;

    void init(DWARFBuffer &dwarfBuffer, std::shared_ptr<AbbrevObject> abbrevObject);

    DWARFInfoSpec getSpec();

    int64_t getSize();

    void write(char *buffer, int64_t &offset);

    void print();

    void fix_offset(
        std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        std::vector<std::unordered_map<int64_t, int64_t>> &DIEOffsetMapVec,
        std::unordered_map<uint64_t, uint64_t> &CUOffsetMap);
    void fix_reference_die_attr(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::vector<std::unordered_map<int64_t, int64_t>> &DIEOffsetMapVec,
        const std::unordered_map<uint64_t, uint64_t> &CUOffsetMap);
    void fix_reference_die_loclists(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::vector<std::unordered_map<int64_t, int64_t>> &DIEOffsetMapVec,
        std::shared_ptr<LoclistsObject> &loclistsObject);
};

#endif