#include <stdint.h>                // for int64_t, uint64_t, int16_t
#include <iostream>                // for operator<<, basic_ostream, cout
#include <iterator>                // for next
#include <memory>                  // for shared_ptr
#include <numeric>                 // for accumulate
#include <unordered_map>           // for unordered_map
#include <unordered_set>           // for unordered_set
#include <vector>                  // for vector
#include "DWARF/AbbrevItem.h"      // for AbbrevItem
#include "DWARF/CompilationUnit.h" // for CompilationUnit
#include "DWARF/DWARFBuffer.h"     // for DWARFBuffer
#include "DWARF/DWARFOP.h"
#include "DWARF/InfoEntry.h"  // for InfoEntry
#include "DWARF/InfoObject.h" // for InfoObject, DWARFInfoSpec
#include "Fig/panic.hpp"      // for FIG_PANIC
#include "Utils/Struct.h"     // for dr_immut
#include "libdwarf/dwarf.h"   // for DW_TAG_array_type, DW_TAG_base_type
struct AbbrevObject;          // lines 16-16
struct LoclistsObject;        // lines 17-17

InfoObject::InfoObject(DWARFBuffer &dwarfBuffer, std::shared_ptr<AbbrevObject> abbrevObject) {
    this->init(dwarfBuffer, abbrevObject);
}

void InfoObject::init(DWARFBuffer &dwarfBuffer, std::shared_ptr<AbbrevObject> abbrevObject) {
    /* 初始化abbrevObjectRef */
    this->abbrevObjectRef = abbrevObject;
    char *infobuffer      = dwarfBuffer.infoBuffer.get();
    int64_t infoSize      = dwarfBuffer.infoSize.get();
    int64_t infoOffset    = 0;
    int16_t module        = 1;
    while(infoSize - infoOffset >= 12) {
        this->compilationUnits.emplace_back(infobuffer, infoOffset, this->abbrevObjectRef, module++);
    }
}

void InfoObject::fix_offset(
    std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    std::vector<std::unordered_map<int64_t, int64_t>> &DIEOffsetMapVec,
    std::unordered_map<uint64_t, uint64_t> &CUOffsetMap) {
    int64_t curCUOffset = 0;
    for(auto &cu : this->compilationUnits) {
        CUOffsetMap.emplace(static_cast<uint64_t>(curCUOffset), cu.offset);
        DIEOffsetMapVec.push_back(std::unordered_map<int64_t, int64_t>());
        cu.fix_offset(curCUOffset, GlobalOffsetMap, DIEOffsetMapVec.back());
        curCUOffset += cu.getSize();
    }
}

void InfoObject::fix_reference_die_loclists(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::vector<std::unordered_map<int64_t, int64_t>> &DIEOffsetMapVec,
    std::shared_ptr<LoclistsObject> &loclistsObject) {
    if(this->compilationUnits.size() != DIEOffsetMapVec.size())
        FIG_PANIC("compilationUnits.size() != DIEOffsetMapVec.size()");

    std::unordered_set<uint64_t> loclistOffsetFixedSet;
    auto cuIterator     = this->compilationUnits.begin();
    auto DIEMapIterator = DIEOffsetMapVec.begin();
    while(cuIterator != this->compilationUnits.end()) {
        cuIterator->fix_reference_die_loclists(GlobalOffsetMap, *DIEMapIterator, loclistOffsetFixedSet, loclistsObject);
        cuIterator     = std::next(cuIterator);
        DIEMapIterator = std::next(DIEMapIterator);
    }
}

void InfoObject::fix_reference_die_attr(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::vector<std::unordered_map<int64_t, int64_t>> &DIEOffsetMapVec,
    const std::unordered_map<uint64_t, uint64_t> &CUOffsetMap) {
    if(this->compilationUnits.size() != DIEOffsetMapVec.size())
        FIG_PANIC("compilationUnits.size() != DIEOffsetMapVec.size()");

    auto cuIterator     = this->compilationUnits.begin();
    auto DIEMapIterator = DIEOffsetMapVec.begin();
    while(cuIterator != this->compilationUnits.end()) {
        cuIterator->fix_reference_die_attr(GlobalOffsetMap, *DIEMapIterator, CUOffsetMap);
        cuIterator     = std::next(cuIterator);
        DIEMapIterator = std::next(DIEMapIterator);
    }
}

void InfoObject::print() {
    std::cout << "Contents of the .debug_info section:\n\n";
    for(auto &cu : this->compilationUnits) {
        cu.print();
    }
}

int64_t InfoObject::getSize() {
    int64_t res = std::accumulate(
        this->compilationUnits.begin(), this->compilationUnits.end(), 0,
        [](int64_t accumulator, CompilationUnit &cu) -> int64_t {
            return accumulator + cu.getSize();
        });
    return res;
}

void _getspec(const InfoEntry &entry, DWARFInfoSpec &spec) {
    if(DWARF::isFuncTag(entry.abbrevItem.tag)) {
        spec.funcAttrsCount += entry.get_all_attr_count();
        for(const auto &childEntry : entry.children) {
            _getspec(childEntry, spec);
        }
    } else if(DWARF::isVariableTag(entry.abbrevItem.tag) || DWARF::isParameterTag(entry.abbrevItem.tag)) {
        spec.varAttrsCount += entry.get_all_attr_count();
    } else if(DWARF::isTypeTag(entry.abbrevItem.tag)) {
        spec.typeAttrsCount += entry.get_all_attr_count();
    } else {
        spec.othersAttrsCount += entry.get_attr_count();
        for(const auto &childEntry : entry.children) {
            _getspec(childEntry, spec);
        }
    }
}

DWARFInfoSpec InfoObject::getSpec() {
    DWARFInfoSpec spec;
    for(const auto &cu : this->compilationUnits) {
        _getspec(cu.infoEntry, spec);
        spec.allAttrsCount += cu.infoEntry.get_all_attr_count();
    }
    return spec;
}

void InfoObject::write(char *buffer, int64_t &offset) {
    for(auto &cu : this->compilationUnits) {
        cu.write(buffer, offset);
    }
}
