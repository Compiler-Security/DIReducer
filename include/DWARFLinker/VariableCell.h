#ifndef DWARFLINKER_VARIABLECELL_H
#define DWARFLINKER_VARIABLECELL_H

#include <cstdint>            // for uint32_t, uint64_t
#include <memory>             // for shared_ptr
#include "DWARF/AttrBuffer.h" // for AttrBuffer
#include "DWARF/InfoEntry.h"  // for InfoEntry

namespace DWARFLinker {
    struct TypeCell;
    struct VariableCell;
}; // namespace DWARFLinker

struct DWARFLinker::VariableCell {
    uint64_t DWARFLoc;
    bool isParameter;

    // Actual Data

    std::shared_ptr<DWARFLinker::TypeCell> typeCell;
    AttrBuffer location;

    uint64_t lowPC;
    uint64_t rangeLength;

    VariableCell() = default;

    ~VariableCell() = default;

    InfoEntry getInfoEntry() const;
};

#endif