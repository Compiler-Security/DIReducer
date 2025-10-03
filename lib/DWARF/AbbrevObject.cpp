#include "DWARF/AbbrevObject.h"
#include <iostream> // for char_traits, basic_ostream, operator<<
#include <numeric>
#include <utility>             // for pair, make_pair
#include "DWARF/AbbrevList.h"  // for abbrev_list
#include "DWARF/DWARFBuffer.h" // for dwarf_buffer
#include "Fig/panic.hpp"
#include "Utils/Struct.h" // for dr_immut

AbbrevObject::AbbrevObject(DWARFBuffer &dwarfBuffer) {
    this->init(dwarfBuffer);
}

void AbbrevObject::init(DWARFBuffer &dwarfBuffer) {
    char *abbrevbuffer   = dwarfBuffer.abbrevBuffer.get();
    int64_t abbrevSize   = dwarfBuffer.abbrevSize.get();
    int64_t abbrevOffset = 0;
    while(abbrevOffset < abbrevSize) {
        int64_t originOffset = abbrevOffset;
        this->abbrevlistMap.insert(
            std::make_pair(
                originOffset,
                new AbbrevList(abbrevbuffer, abbrevOffset)));
    }
}

std::shared_ptr<AbbrevList> AbbrevObject::getAbbrevList(uint64_t offset) {
    if(this->abbrevlistMap.find(offset) != this->abbrevlistMap.end()) {
        return this->abbrevlistMap.at(offset);
    } else {
        FIG_PANIC("Offset ", offset, " is not found in the abbrev object");
    }
}

int64_t AbbrevObject::getSize() {
    int64_t res = std::accumulate(
        this->abbrevlistMap.begin(), this->abbrevlistMap.end(), 0,
        [](int64_t accumulator, const std::pair<uint64_t, std::shared_ptr<AbbrevList>> &it) -> int64_t {
            return accumulator + it.second->getSize();
        });
    return res;
}

void AbbrevObject::write(char *buffer, int64_t &offset) {
    for(auto &it : this->abbrevlistMap) {
        it.second->write(buffer, offset);
    }
}

void AbbrevObject::fix_offset() {
    auto originAbbrevObject = this->abbrevlistMap;
    this->abbrevlistMap.clear();
    int64_t curOffset = 0;
    for(auto &it : originAbbrevObject) {
        it.second->offset = curOffset;
        this->abbrevlistMap.insert(std::make_pair(curOffset, it.second));
        curOffset += it.second->getSize();
    }
}

void AbbrevObject::print() {
    std::cout << "Contents of the .debug_abbrev section:" << std::endl
              << std::endl;
    for(auto &it : this->abbrevlistMap) {
        std::cout << "Number TAG (" << std::hex << it.first << ")" << std::dec << std::endl;
        it.second->print();
    }
}