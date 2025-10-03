#ifndef DWARFLINKER_TYPECELL_H
#define DWARFLINKER_TYPECELL_H

#include <cstdint>           // for uint32_t, uint64_t
#include <memory>            // for shared_ptr
#include <optional>          // for optional
#include <string>            // for string
#include <vector>            // for vector
#include "DWARF/InfoEntry.h" // for InfoEntry

namespace CFG {
    enum class BASETYPE;
    enum class TYPETAG;
}; // namespace CFG

namespace DWARFLinker {
    struct TypeCell;
    struct TypeMember;
}; // namespace DWARFLinker

struct DWARFLinker::TypeMember {
    // Actual Data

    std::shared_ptr<DWARFLinker::TypeCell> type;
    bool isBitLocation;
    uint32_t memberLocation;

    /// @brief 获取该Member对应的InfoEntry
    InfoEntry getInfoEntry() const;
};

struct DWARFLinker::TypeCell {
    uint32_t sn;
    std::string name;
    uint64_t DWARFLoc;

    // Actual Data

    CFG::TYPETAG tag;
    // all type
    uint32_t size;
    // basetype
    CFG::BASETYPE basetype;
    // array pointer function
    std::optional<std::shared_ptr<DWARFLinker::TypeCell>> refTypeCell;
    // array
    uint64_t arrayBound;
    // struct union
    std::optional<std::vector<TypeMember>> typeMember;

    TypeCell() = default;
    TypeCell(CFG::TYPETAG tag);

    ~TypeCell() = default;

    InfoEntry getInfoEntry();
};

#endif