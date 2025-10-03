#include <stdint.h>                 // for uint64_t, int64_t, uint32_t
#include <cstring>                  // for memcpy
#include <vector>                   // for vector
#include "DWARF/DWARFBuffer.h"     // for dwarf_buffer
#include "DWARF/LoclistsList.h"    // for loclists_list
#include "DWARF/LoclistsObject.h"  // for loclists_object
#include "Fig/panic.hpp"            // for FIG_PANIC
#include "Utils/Struct.h"        // for dr_immut
#include "Utils/Common.h"         // for dec_to_hex

LoclistsObject::LoclistsObject(DWARFBuffer &dwarfBuffer) {
    this->init(dwarfBuffer);
}

void LoclistsObject::init(DWARFBuffer &dwarfBuffer) {
    if(dwarfBuffer.loclistsBuffer.is_empty())
        FIG_PANIC("There is not .debug_loclists section, failed to initialize");

    this->loclistsBuffer.set(dwarfBuffer.loclistsBuffer.get());
    this->loclistsSize     = dwarfBuffer.loclistsSize.get();
    int64_t loclistsoffset = 0;
    while(loclistsoffset < this->loclistsSize) {
        this->loclistsObject.emplace_back(this->loclistsBuffer.get(), loclistsoffset);
    }
}

uint64_t LoclistsObject::get_target_loclist_offset(uint64_t loclists_base, uint64_t relative_offset) {
    for(auto &loclists : this->loclistsObject) {
        if(loclists.offsetArrayBase != loclists_base)
            continue;

        uint32_t index;
        if(loclists.bitWidth == 64)
            index = relative_offset / 8;
        else
            index = relative_offset / 4;

        if(index >= loclists.offsetEntryCount)
            FIG_PANIC("index: ", index, ",  offsetEntryCount: ", loclists.offsetEntryCount, "\n", "Out of Range!");

        return loclists_base + loclists.offsetArray[index];
    }

    FIG_PANIC("Can not find suitable offsetArrayBase for ", dec_to_hex(loclists_base));
}

void LoclistsObject::print() {
    for(auto &loclist : this->loclistsObject) {
        loclist.print();
    }
}

void LoclistsObject::write(char *buffer, int64_t &offset) {
    std::memcpy(buffer + offset, this->loclistsBuffer.get(), this->loclistsSize);
    offset += this->loclistsSize;
}