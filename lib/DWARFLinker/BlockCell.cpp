#include "DWARFLinker/BlockCell.h"
#include "DWARFLinker/Linker.h"
#include "Utils/Buffer.h"

namespace DWARFLinker {

    BlockCell::BlockCell(uint64_t lowPc, uint64_t rangeLength) :
        lowPC(lowPc), rangeLength(rangeLength) {
    }

    /// | DW_AT_low_pc, DW_FORM_data4
    /// | DW_AT_high_pc, DW_FORM_data4
    InfoEntry BlockCell::getInfoEntry() const {
        InfoEntry infoEntry;
        infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::Block);
        // | DW_AT_low_pc, DW_FORM_data4
        AttrBuffer lowPcBuffer = AttrBuffer();
        lowPcBuffer.buffer     = std::make_unique<char[]>(4);
        lowPcBuffer.size       = 4;
        int64_t tmpOffset      = 0;
        write_u32(this->lowPC, lowPcBuffer.buffer.get(), tmpOffset);
        infoEntry.attrBuffers.push_back(std::move(lowPcBuffer));
        // | DW_AT_high_pc, DW_FORM_data4
        AttrBuffer highPcBuffer = AttrBuffer();
        highPcBuffer.buffer     = std::make_unique<char[]>(4);
        highPcBuffer.size       = 4;
        tmpOffset               = 0;
        write_u32(this->rangeLength, highPcBuffer.buffer.get(), tmpOffset);
        infoEntry.attrBuffers.push_back(std::move(highPcBuffer));

        // variables
        auto &children = infoEntry.children;
        for(const auto &varCell : varCells) {
            children.push_back(varCell->getInfoEntry());
        }

        children.push_back(InfoEntry());
        children.back().abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::EndItem);

        return infoEntry;
    }

} // namespace DWARFLinker