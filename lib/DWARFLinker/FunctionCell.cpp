#include "DWARFLinker/FunctionCell.h"
#include <cstdint>
#include <map>
#include <memory>             // for shared_ptr, make_unique, uniqu...
#include <utility>            // for move
#include "DWARF/AttrBuffer.h" // for AttrBuffer
#include "DWARF/InfoEntry.h"  // for InfoEntry
#include "DWARFLinker/BlockCell.h"
#include "DWARFLinker/Linker.h" // for AbbrevNumBer
#include "Fig/assert.hpp"       // for FIG_ASSERT
#include "Utils/Buffer.h"       // for write_u32

namespace DWARFLinker {

    /// | DW_AT_low_pc, DW_FORM_data4
    /// | DW_AT_high_pc, DW_FORM_data4
    /// [Option] | DW_AT_type, DW_FORM_ref4
    /// | DW_AT_frame_base, DW_FORM_exprloc
    InfoEntry FunctionCell::getInfoEntry() const {
        InfoEntry infoEntry;
        bool isWithSubNode = (this->argCells.size() != 0 || this->varCells.size() != 0);
        bool isVoid        = (!this->returnTypeCell.has_value());
        if(isWithSubNode && isVoid)
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::FuncVoidWithSubNode);
        else if(isWithSubNode && !isVoid)
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::FuncWithSubNode);
        else if(!isWithSubNode && isVoid)
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::FuncVoidWithoutSubNode);
        else if(!isWithSubNode && !isVoid)
            infoEntry.abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::FuncWithoutSubNode);
        // | DW_AT_low_pc, DW_FORM_data4
        AttrBuffer lowPcBuffer = AttrBuffer();
        lowPcBuffer.buffer     = std::make_unique<char[]>(4);
        lowPcBuffer.size       = 4;
        int64_t tmpOffset      = 0;
        write_u32(this->address, lowPcBuffer.buffer.get(), tmpOffset);
        infoEntry.attrBuffers.push_back(std::move(lowPcBuffer));
        // | DW_AT_high_pc, DW_FORM_data4
        AttrBuffer hignPcBuffer = AttrBuffer();
        hignPcBuffer.buffer     = std::make_unique<char[]>(4);
        hignPcBuffer.size       = 4;
        tmpOffset               = 0;
        write_u32(this->size, hignPcBuffer.buffer.get(), tmpOffset);
        infoEntry.attrBuffers.push_back(std::move(hignPcBuffer));
        // Option | DW_AT_type, DW_FORM_ref4
        if(!isVoid) {
            FIG_ASSERT(this->returnTypeCell.value().get() != nullptr);
            AttrBuffer typeBuffer = AttrBuffer();
            typeBuffer.buffer     = std::make_unique<char[]>(4);
            typeBuffer.size       = 4;
            tmpOffset             = 0;
            write_u32(this->returnTypeCell.value()->DWARFLoc, typeBuffer.buffer.get(), tmpOffset);
            infoEntry.attrBuffers.push_back(std::move(typeBuffer));
        }
        // | DW_AT_frame_base, DW_FORM_exprloc
        // AttrBuffer frameBaseBuffer = AttrBuffer();
        // frameBaseBuffer.buffer     = std::make_unique<char[]>(2);
        // frameBaseBuffer.size       = 2;
        // tmpOffset                  = 0;
        // write_leb128u(1, frameBaseBuffer.buffer.get(), tmpOffset);
        // write_u8(0x9c, frameBaseBuffer.buffer.get(), tmpOffset);
        // FIG_ASSERT(tmpOffset == 2, "frameBaseBuffer size is not 2!");

        // parameters and variables
        if(isWithSubNode) {
            auto &children = infoEntry.children;
            // add parameters
            for(const auto &argCell : this->argCells) {
                children.push_back(argCell->getInfoEntry());
            }

            // Variable Range Spilt
            // 1. function's scope variables (including above parameters) => as children of function DIE
            // 2. function's lexical block variables => as children of lexical block DIE & lexical block DIE as children of function DIE
            std::map<std::pair<uint64_t, uint64_t>, BlockCell> blockCellMap;
            for(const auto &varCell : this->varCells) {
                if(varCell->lowPC == this->address && varCell->rangeLength == this->size) {
                    children.push_back(varCell->getInfoEntry());
                } else {
                    auto blockRange = std::make_pair(varCell->lowPC, varCell->rangeLength);
                    if(blockCellMap.find(blockRange) == blockCellMap.end()) {
                        BlockCell blockCell{blockRange.first, blockRange.second};
                        blockCell.varCells.push_back(varCell);
                        blockCellMap[blockRange] = std::move(blockCell);
                    } else {
                        blockCellMap[blockRange].varCells.push_back(varCell);
                    }
                }
            }

            for(const auto &blockCell : blockCellMap) {
                children.push_back(blockCell.second.getInfoEntry());
            }

            children.push_back(InfoEntry());
            children.back().abbrevNumber = static_cast<uint64_t>(AbbrevNumBer::EndItem);
        }

        return infoEntry;
    }
} // namespace DWARFLinker