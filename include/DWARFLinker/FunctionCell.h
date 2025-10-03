#ifndef DWARFLINKER_FUNCTIONCELL_H
#define DWARFLINKER_FUNCTIONCELL_H

#include <cstdint>           // for uint64_t
#include <memory>            // for shared_ptr
#include <optional>          // for optional
#include <vector>            // for vector
#include "DWARF/InfoEntry.h" // for InfoEntry

namespace DWARFLinker {
    struct TypeCell;
    struct VariableCell;
    struct FunctionCell;
} // namespace DWARFLinker

struct DWARFLinker::FunctionCell {
    uint64_t DWARFLoc;

    // Actual Data

    uint64_t address;
    uint64_t size;
    // void <=> nullptr
    std::optional<std::shared_ptr<DWARFLinker::TypeCell>> returnTypeCell;
    std::vector<std::shared_ptr<DWARFLinker::VariableCell>> argCells;
    std::vector<std::shared_ptr<DWARFLinker::VariableCell>> varCells;

    FunctionCell() = default;

    ~FunctionCell() = default;

    InfoEntry getInfoEntry() const;
};

#endif