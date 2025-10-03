#ifndef BLOCKCELL_H
#define BLOCKCELL_H

#include "DWARF/InfoEntry.h"
#include <cstdint>

namespace DWARFLinker {
    struct BlockCell;
    struct VariableCell;
}; // namespace DWARFLinker

struct DWARFLinker::BlockCell {
    uint64_t DWARFLoc;

    uint64_t lowPC;
    uint64_t rangeLength;
    std::vector<std::shared_ptr<DWARFLinker::VariableCell>> varCells;

    BlockCell() = default;
    BlockCell(uint64_t lowPc, uint64_t rangeLength);
    ~BlockCell() = default;

    /// @brief
    InfoEntry getInfoEntry() const;
};

#endif // BLOCKCELL_H