#include "DWARFLinker/VariableCell.h"
#include <cstdint>                // for uint64_t, int64_t, uint32_t
#include <utility>                // for move
#include <vector>                 // for vector
#include "DWARFLinker/Linker.h"   // for AbbrevNumBer
#include "Fig/assert.hpp"         // for FIG_ASSERT
#include "Utils/Buffer.h"         // for write_u32

namespace DWARFLinker {
    /// | DW_AT_location, DW_FORM_exprloc
    /// | DW_AT_type, DW_FORM_ref4
    InfoEntry VariableCell::getInfoEntry() const {
        // | DW_AT_location, DW_FORM_exprloc
        InfoEntry infoEntry;
        if(this->isParameter)
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::Parameter);
        else
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::Variable);
        infoEntry.attrBuffers.push_back(this->location);
        // | DW_AT_type, DW_FORM_ref4
        FIG_ASSERT(this->typeCell.get() != nullptr);
        AttrBuffer typeBuffer = AttrBuffer();
        typeBuffer.buffer     = std::make_unique<char[]>(4);
        typeBuffer.size       = 4;
        int64_t tmpOffset     = 0;
        write_u32(this->typeCell->DWARFLoc, typeBuffer.buffer.get(), tmpOffset);
        infoEntry.attrBuffers.push_back(std::move(typeBuffer));

        return infoEntry;
    }
}; // namespace DWARFLinker