#include <cstdint>                 // for int64_t, uint64_t, uint32_t, uint...
#include <cstring>                 // for memcpy
#include <iostream>                // for basic_ostream, operator<<, cout, dec
#include <iterator>                // for next
#include <memory>                  // for unique_ptr, shared_ptr, __shared_...
#include <numeric>                 // for accumulate
#include <string>                  // for char_traits, string
#include <unordered_map>           // for unordered_map, operator==, _Node_...
#include <unordered_set>           // for unordered_set
#include <utility>                 // for pair, make_pair
#include <vector>                  // for vector
#include "DWARF/AbbrevEntry.h"     // for AbbrevEntry
#include "DWARF/AbbrevItem.h"      // for AbbrevItem
#include "DWARF/AbbrevList.h"      // for AbbrevList
#include "DWARF/AbbrevParser.h"    // for AbbrevParser
#include "DWARF/AttrBuffer.h"      // for AttrBuffer
#include "DWARF/DWARFExprParser.h" // for DWARFExprParser
#include "DWARF/ELFObject.h"       // for StrTable
#include "DWARF/InfoEntry.h"       // for InfoEntry, get_dw_at_abs_ori, get...
#include "DWARF/LoclistsEntry.h"   // for LoclistsEntry
#include "DWARF/LoclistsObject.h"  // for LoclistsObject
#include "Fig/assert.hpp"          // for FIG_ASSERT
#include "Fig/panic.hpp"           // for FIG_PANIC
#include "Utils/Buffer.h"          // for read_u32, read_u64, read_u8, read...
#include "Utils/Common.h"          // for dec_to_hex
#include "Utils/Struct.h"          // for dr_immut
#include "Utils/ToStr.h"           // for dw_form_to_str, dw_at_to_str, dw_...
#include "libdwarf/dwarf.h"        // for DW_FORM_data4, DW_FORM_data1, DW_...

InfoEntry::InfoEntry(
    const char *const buffer,
    int64_t &offset,
    std::shared_ptr<AbbrevParser> abbrevParser,
    const std::shared_ptr<AbbrevList> &abbrevList,
    uint16_t depth,
    uint16_t module) {
    this->init(buffer, offset, abbrevParser, abbrevList, depth, module);
}

bool InfoEntry::isNull() const {
    return this->abbrevNumber == 0;
}

int64_t InfoEntry::get_abbrev_num_size() const {
    return get_uint_length_leb128(this->abbrevNumber);
}

int64_t InfoEntry::get_raw_data_size() const {
    return std::accumulate(
        this->attrBuffers.begin(), this->attrBuffers.end(), 0,
        [](int64_t accumulator, const AttrBuffer &buffer) -> int64_t {
            return accumulator + buffer.size;
        });
}

int64_t InfoEntry::get_children_size() const {
    return std::accumulate(
        this->children.begin(), this->children.end(), 0,
        [](int64_t accumulator, const InfoEntry &child) -> int64_t {
            return accumulator + child.getSize();
        });
}

int64_t InfoEntry::get_all_attr_count() const {
    int64_t cur_count   = this->get_attr_count();
    int64_t child_count = std::accumulate(
        this->children.begin(), this->children.end(), 0,
        [](int64_t accumulator, const InfoEntry &child) -> int64_t {
            return accumulator + child.get_all_attr_count();
        });

    return cur_count + child_count;
}

int64_t InfoEntry::get_attr_count() const {
    int64_t res = 0;
    if(this->isNull())
        res =  0;
    else
        res = this->attrBuffers.size();

    FIG_ASSERT(res >= 0, "loc: ", dec_to_hex(this->get_global_offset()));

    return res;
}

uint64_t InfoEntry::get_global_offset() const {
    return this->cuOffset + this->innerOffset;
}
uint64_t InfoEntry::get_local_offset() const {
    return this->innerOffset;
}

int64_t InfoEntry::getSize() const {
    int64_t res = this->get_abbrev_num_size() + this->get_raw_data_size() + this->get_children_size();
    return res;
}

void InfoEntry::fix_offset(
    const int64_t &curCUOffset,
    int64_t &curInnerOffset,
    std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    std::unordered_map<int64_t, int64_t> &DIEOffsetMap) {
    GlobalOffsetMap.insert(std::make_pair(this->cuOffset + this->innerOffset, curCUOffset + curInnerOffset));
    DIEOffsetMap.insert(std::make_pair(this->innerOffset, curInnerOffset));
    this->cuOffset    = curCUOffset;
    this->innerOffset = curInnerOffset;
    curInnerOffset += this->get_abbrev_num_size();
    curInnerOffset += this->get_raw_data_size();
    for(auto &child : this->children) {
        child.fix_offset(curCUOffset, curInnerOffset, GlobalOffsetMap, DIEOffsetMap);
    }
}

void InfoEntry::fix_reference_die_loclists(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    std::unordered_set<uint64_t> &loclistOffsetFixedSet,
    std::shared_ptr<LoclistsObject> &loclistsObject,
    uint64_t loclists_base) {
    if(this->isNull())
        return;

    auto bufferIterator = this->attrBuffers.begin();
    auto entryIterator  = this->abbrevItem.entries.begin();
    while(!entryIterator->isNull()) {
        switch(entryIterator->at) {
        case DW_AT_location:
        case DW_AT_string_length:
        case DW_AT_return_addr:
        case DW_AT_data_member_location:
        case DW_AT_frame_base:
        case DW_AT_segment:
        case DW_AT_static_link:
        case DW_AT_use_location:
        case DW_AT_vtable_elem_location: {
            if(entryIterator->form == DW_FORM_sec_offset || entryIterator->form == DW_FORM_loclistx) {
                this->__fix_reference_loclists(
                    GlobalOffsetMap, DIEOffsetMap, bufferIterator, loclistsObject,
                    loclistOffsetFixedSet, loclists_base, entryIterator->form);
            }
            break;
        }
        default: break;
        }

        bufferIterator = std::next(bufferIterator);
        entryIterator  = std::next(entryIterator);
    }

    for(auto &child : this->children) {
        child.fix_reference_die_loclists(GlobalOffsetMap, DIEOffsetMap, loclistOffsetFixedSet, loclistsObject, loclists_base);
    }
}

void InfoEntry::fix_reference_die_attr(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::unordered_map<uint64_t, uint64_t> &CUOffsetMap) {
    if(this->isNull())
        return;

    auto bufferIterator = this->attrBuffers.begin();
    auto entryIterator  = this->abbrevItem.entries.begin();
    while(!entryIterator->isNull()) {
        switch(entryIterator->form) {
        case DW_FORM_ref1: this->__fix_reference_ref1(DIEOffsetMap, bufferIterator); break;
        case DW_FORM_ref2: this->__fix_reference_ref2(DIEOffsetMap, bufferIterator); break;
        case DW_FORM_ref4: this->__fix_reference_ref4(DIEOffsetMap, bufferIterator); break;
        case DW_FORM_ref8: this->__fix_reference_ref8(DIEOffsetMap, bufferIterator); break;
        case DW_FORM_ref_udata: this->__fix_reference_ref_udata(DIEOffsetMap, bufferIterator); break;
        case DW_FORM_ref_addr: this->__fix_reference_ref_addr(GlobalOffsetMap, bufferIterator); break;
        case DW_FORM_ref_sup4: FIG_PANIC("unsupported relocatable form: ", dw_form_to_str(entryIterator->form)); break;
        case DW_FORM_ref_sup8: FIG_PANIC("unsupported relocatable form: ", dw_form_to_str(entryIterator->form)); break;
        case DW_FORM_exprloc: this->__fix_reference_exprloc(GlobalOffsetMap, DIEOffsetMap, bufferIterator); break;
        default: break;
        }

        bufferIterator = std::next(bufferIterator);
        entryIterator  = std::next(entryIterator);
    }

    for(auto &child : this->children) {
        child.fix_reference_die_attr(GlobalOffsetMap, DIEOffsetMap, CUOffsetMap);
    }
}

void InfoEntry::__fix_reference_ref1(
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator) {
    int64_t tmpOffset      = 0;
    int64_t oldInnerOffset = read_u8(bufferIterator->buffer.get(), tmpOffset);
    FIG_ASSERT(DIEOffsetMap.find(oldInnerOffset) != DIEOffsetMap.end(),
               "Old Global Offset: 0x", dec_to_hex(oldInnerOffset), " not found in DIEOffsetMap");

    tmpOffset = 0;
    auto cit  = DIEOffsetMap.find(oldInnerOffset);
    write_u8(cit->second, bufferIterator->buffer.get(), tmpOffset);
}

void InfoEntry::__fix_reference_ref2(
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator) {
    int64_t tmpOffset      = 0;
    int64_t oldInnerOffset = read_u16(bufferIterator->buffer.get(), tmpOffset);
    FIG_ASSERT(DIEOffsetMap.find(oldInnerOffset) != DIEOffsetMap.end(),
               "Old Global Offset: 0x", dec_to_hex(oldInnerOffset), " not found in DIEOffsetMap");

    tmpOffset = 0;
    auto cit  = DIEOffsetMap.find(oldInnerOffset);
    write_u16(cit->second, bufferIterator->buffer.get(), tmpOffset);
}

void InfoEntry::__fix_reference_ref4(
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator) {
    int64_t tmpOffset      = 0;
    int64_t oldInnerOffset = read_u32(bufferIterator->buffer.get(), tmpOffset);
    FIG_ASSERT(DIEOffsetMap.find(oldInnerOffset) != DIEOffsetMap.end(),
               "Old Global Offset: 0x", dec_to_hex(oldInnerOffset), " not found in DIEOffsetMap");

    tmpOffset = 0;
    auto cit  = DIEOffsetMap.find(oldInnerOffset);
    write_u32(cit->second, bufferIterator->buffer.get(), tmpOffset);
}

void InfoEntry::__fix_reference_ref8(
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator) {
    int64_t tmpOffset      = 0;
    int64_t oldInnerOffset = read_u64(bufferIterator->buffer.get(), tmpOffset);
    FIG_ASSERT(DIEOffsetMap.find(oldInnerOffset) != DIEOffsetMap.end(),
               "Old Global Offset: 0x", dec_to_hex(oldInnerOffset), " not found in DIEOffsetMap");

    tmpOffset = 0;
    auto cit  = DIEOffsetMap.find(oldInnerOffset);
    write_u64(cit->second, bufferIterator->buffer.get(), tmpOffset);
}

void InfoEntry::__fix_reference_ref_udata(
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator) {
    int64_t tmpOffset      = 0;
    int64_t oldInnerOffset = read_leb128u(bufferIterator->buffer.get(), tmpOffset);
    FIG_ASSERT(DIEOffsetMap.find(oldInnerOffset) != DIEOffsetMap.end(),
               "Old Global Offset: 0x", dec_to_hex(oldInnerOffset), " not found in DIEOffsetMap");

    tmpOffset = 0;
    auto cit  = DIEOffsetMap.find(oldInnerOffset);
    write_leb128u_fixed_length(cit->second, bufferIterator->buffer.get(), tmpOffset, bufferIterator->size);
}

void InfoEntry::__fix_reference_ref_addr(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator) {
    uint32_t bufferSize = bufferIterator->size;
    if(bufferSize != 4 && bufferSize != 8)
        FIG_PANIC("bad buffer size for DW_AT_ref_addr");

    int64_t tmpOffset       = 0;
    int64_t oldGlobalOffset = 0;
    if(bufferSize == 4)
        oldGlobalOffset = read_u32(bufferIterator->buffer.get(), tmpOffset);
    else
        oldGlobalOffset = read_u64(bufferIterator->buffer.get(), tmpOffset);

    FIG_ASSERT(GlobalOffsetMap.find(oldGlobalOffset) != GlobalOffsetMap.end(),
               "Old Global Offset: 0x", dec_to_hex(oldGlobalOffset), " not found in GlobalOffsetMap");

    tmpOffset = 0;
    auto cit  = GlobalOffsetMap.find(oldGlobalOffset);
    if(bufferSize == 4)
        write_u32(cit->second, bufferIterator->buffer.get(), tmpOffset);
    else
        write_u64(cit->second, bufferIterator->buffer.get(), tmpOffset);
}

void InfoEntry::__fix_reference_exprloc(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator) {
    int64_t tmpOffset  = 0;
    uint64_t blockSize = read_leb128u(bufferIterator->buffer.get(), tmpOffset);
    DWARFExprParser dwarfExprParser(bufferIterator->buffer.get() + tmpOffset, blockSize, this->abbrevParser->bitWidth, this->abbrevParser->addressSize);
    dwarfExprParser.fix_reference_to_die(GlobalOffsetMap, DIEOffsetMap);
    dwarfExprParser.write(bufferIterator->buffer.get(), tmpOffset);
}

void InfoEntry::__fix_reference_loclists(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::vector<AttrBuffer>::iterator &bufferIterator,
    const std::shared_ptr<LoclistsObject> &loclistsObject,
    std::unordered_set<uint64_t> &loclistOffsetFixedSet,
    uint64_t loclists_base,
    uint64_t form) {
    if(form != DW_FORM_sec_offset && form != DW_FORM_loclistx)
        FIG_PANIC("STOP");

    int64_t tmpOffset = 0;
    int64_t loclistsOffset;
    if(form == DW_FORM_sec_offset) { 
        if(bufferIterator->size == 8)
            loclistsOffset = read_u64(bufferIterator->buffer.get(), tmpOffset);
        else
            loclistsOffset = read_u32(bufferIterator->buffer.get(), tmpOffset);
    } else { 
        uint64_t relative_offset = read_leb128u(bufferIterator->buffer.get(), tmpOffset);
        loclistsOffset           = loclistsObject->get_target_loclist_offset(loclists_base, relative_offset);
    }

    if(loclistOffsetFixedSet.find(loclistsOffset) != loclistOffsetFixedSet.end())
        return;

    loclistOffsetFixedSet.insert(loclistsOffset);
    LoclistsEntry loclistsEntry(
        loclistsObject->loclistsBuffer.get(), loclistsOffset, this->abbrevParser->addressSize, this->abbrevParser->bitWidth);

    if(loclistsEntry.type == DW_LLE_startx_endx || loclistsEntry.type == DW_LLE_startx_length || loclistsEntry.type == DW_LLE_offset_pair || loclistsEntry.type == DW_LLE_default_location || loclistsEntry.type == DW_LLE_start_end || loclistsEntry.type == DW_LLE_start_length) {
        DWARFExprParser dwarfExprParser(
            loclistsEntry.blockBuffer, loclistsEntry.blockSize, this->abbrevParser->bitWidth, this->abbrevParser->addressSize);
        tmpOffset = 0;
        dwarfExprParser.fix_reference_to_die(GlobalOffsetMap, DIEOffsetMap);
        dwarfExprParser.write(loclistsEntry.blockBuffer, tmpOffset);
        loclistsEntry.init(
            loclistsObject->loclistsBuffer.get(), loclistsOffset, this->abbrevParser->addressSize, this->abbrevParser->bitWidth);
    }

    while(loclistsEntry.type != DW_LLE_end_of_list) {
        DWARFExprParser dwarfExprParser(
            loclistsEntry.blockBuffer, loclistsEntry.blockSize, this->abbrevParser->bitWidth, this->abbrevParser->addressSize);
        tmpOffset = 0;
        dwarfExprParser.fix_reference_to_die(GlobalOffsetMap, DIEOffsetMap);
        dwarfExprParser.write(loclistsEntry.blockBuffer, tmpOffset);
        loclistsEntry.init(
            loclistsObject->loclistsBuffer.get(), loclistsOffset, this->abbrevParser->addressSize, this->abbrevParser->bitWidth);
    }
}

void InfoEntry::print() const {
    if(this->isNull()) {
        std::cout << " <" << this->depth << ">";
        std::cout << "<" << std::hex << this->cuOffset + this->innerOffset << std::dec << ">";
        std::cout << ": Abbrev Number: " << this->abbrevNumber << std::endl;
        ;
    } else {
        this->print_tag();
        this->print_attr();
        for(auto &child : this->children) {
            child.print();
        }
    }
}

void InfoEntry::print_tag() const {
    std::cout << " <" << this->depth << ">";
    std::cout << "<" << std::hex << this->cuOffset + this->innerOffset << std::dec << ">";
    std::cout << ": Abbrev Number: " << this->abbrevNumber << " ";
    std::cout << "(" << dw_tag_to_str(this->abbrevItem.tag) << ")";
    std::cout << std::endl;
}

void InfoEntry::print_attr() const {
    FIG_ASSERT(this->attrBuffers.size() + 1 == this->abbrevItem.entries.size(),
               "attr size = ", this->attrBuffers.size(), ",  abbrev size = ", this->abbrevItem.entries.size());

    auto attrIterator  = this->attrBuffers.begin();
    auto entryIterator = this->abbrevItem.entries.begin();
    while(!entryIterator->isNull()) {
        std::cout << "    "
                  << "<" << std::hex << attrIterator->offset << std::dec << ">   ";
        std::cout << dw_at_to_str(entryIterator->at) << "\t: ";
        this->abbrevParser->print_abbrev_entry(*entryIterator, attrIterator->buffer.get());
        std::cout << std::endl;
        attrIterator  = std::next(attrIterator);
        entryIterator = std::next(entryIterator);
    }
}

void InfoEntry::init(
    const char *const buffer,
    int64_t &offset,
    std::shared_ptr<AbbrevParser> abbrevParser,
    const std::shared_ptr<AbbrevList> &abbrevList,
    uint16_t depth,
    uint16_t module) {
    this->depth        = depth;
    this->module       = module;
    this->cuOffset     = abbrevParser->cuOffset;
    this->innerOffset  = offset - abbrevParser->cuOffset;
    this->abbrevParser = abbrevParser;
    this->abbrevNumber = read_leb128u(buffer, offset);
    if(this->abbrevNumber == 0)
        return;

    this->abbrevItem = abbrevList->get_abbrev_item_by_number(this->abbrevNumber);

    for(auto &entry : this->abbrevItem.entries) {
        if(!entry.isNull()) {
            uint32_t length = this->abbrevParser->get_length_abbrev_entry(entry, buffer, offset);
            this->attrBuffers.emplace_back(new char[length], offset, length);
            std::memcpy(this->attrBuffers.back().buffer.get(), buffer + offset, length);
            offset += length;
        }
    }

    if(this->abbrevItem.has_children) {
        while(true) {
            this->children.emplace_back(buffer, offset, abbrevParser, abbrevList, this->depth + 1, this->module);
            if(this->children.back().isNull())
                break;
        }
    }
}

void InfoEntry::write(char *buffer, int64_t &offset) {
    write_leb128u(this->abbrevNumber, buffer, offset);
    for(auto &attrBuffer : this->attrBuffers) {
        std::memcpy(buffer + offset, attrBuffer.buffer.get(), attrBuffer.size);
        offset += attrBuffer.size;
    }

    for(auto &child : this->children) {
        child.write(buffer, offset);
    }
}

std::string get_dw_at_name(const AbbrevEntry &entry, const AttrBuffer &buffer, const StrTable &table) {
    switch(entry.form) {
    case DW_FORM_strp: {
        uint64_t str_offset = 0;
        int64_t temp        = 0;
        if(buffer.size == 4) {
            str_offset = read_u32(buffer.buffer.get(), temp);
        } else if(buffer.size == 8) {
            str_offset = read_u64(buffer.buffer.get(), temp);
        } else {
            FIG_PANIC("unsuporrted bitwidth");
        }
        return table.get(str_offset);
    }
    case DW_FORM_string: {
        return std::string(buffer.buffer.get());
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " for DW_AT_name"); break;
    }
}

uint64_t get_dw_at_encoding(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_implicit_const: {
        return entry.implicit_const;
    }
    case DW_FORM_data1: {
        int64_t temp = 0;
        return read_u8(buffer.buffer.get(), temp);
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " for DW_AT_encoding"); break;
    }
}

uint32_t get_dw_at_byte_size(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_implicit_const: {
        return entry.implicit_const;
    }
    case DW_FORM_data1: {
        int64_t temp = 0;
        return read_u8(buffer.buffer.get(), temp);
    }
    case DW_FORM_data2: {
        int64_t temp = 0;
        return read_u16(buffer.buffer.get(), temp);
    }
    case DW_FORM_data4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " for DW_AT_byte_size"); break;
    }
}

uint32_t get_dw_at_data_member_location(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_implicit_const: {
        return entry.implicit_const;
    }
    case DW_FORM_data1: {
        int64_t temp = 0;
        return read_u8(buffer.buffer.get(), temp);
    }
    case DW_FORM_data2: {
        int64_t temp = 0;
        return read_u16(buffer.buffer.get(), temp);
    }
    case DW_FORM_data4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " in get_dw_at_data_member_location"); break;
    }
}

uint32_t get_dw_at_data_bit_offset(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_implicit_const: {
        return entry.implicit_const;
    }
    case DW_FORM_data1: {
        int64_t temp = 0;
        return read_u8(buffer.buffer.get(), temp);
    }
    case DW_FORM_data2: {
        int64_t temp = 0;
        return read_u16(buffer.buffer.get(), temp);
    }
    case DW_FORM_data4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " in get_dw_at_data_bit_offset"); break;
    }
}

uint32_t get_dw_at_bit_size(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_implicit_const: {
        return entry.implicit_const;
    }
    case DW_FORM_data1: {
        int64_t temp = 0;
        return read_u8(buffer.buffer.get(), temp);
    }
    case DW_FORM_data2: {
        int64_t temp = 0;
        return read_u16(buffer.buffer.get(), temp);
    }
    case DW_FORM_data4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " in get_dw_at_bit_size"); break;
    }
}

uint64_t get_dw_at_type(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_ref4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
    }
    case DW_FORM_ref8: {
        int64_t temp = 0;
        return read_u64(buffer.buffer.get(), temp);
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " for DW_AT_type"); break;
    }
}

uint64_t get_dw_at_abs_ori(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_ref4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
    }
    case DW_FORM_ref8: {
        int64_t temp = 0;
        return read_u64(buffer.buffer.get(), temp);
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " for DW_AT_abstract_origin"); break;
    }
}

uint64_t get_dw_at_upper_bound(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_implicit_const: {
        return entry.implicit_const;
    }
    case DW_FORM_data1: {
        int64_t temp = 0;
        return read_u8(buffer.buffer.get(), temp);
    }
    case DW_FORM_data2: {
        int64_t temp = 0;
        return read_u16(buffer.buffer.get(), temp);
    }
    case DW_FORM_data4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
    }
    case DW_FORM_exprloc: {
        // As Unknown
        return 0;
    }
    default: FIG_PANIC("unsuporrted Form ", dw_form_to_str(entry.form), " for DW_AT_upper_bound"); break;
    }
}

uint64_t get_dw_at_low_pc(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_addr: {
        int64_t temp = 0;
        if(buffer.size == 4) {
            return read_u32(buffer.buffer.get(), temp);
        } else if(buffer.size == 8) {
            return read_u64(buffer.buffer.get(), temp);
        } else {
            FIG_PANIC("unsuporrted bitwidth");
        }
        break;
    }
    case DW_FORM_data4: {
        FIG_ASSERT(buffer.size == 4);
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
        break;
    }
    default: FIG_PANIC("unsuporrted Form for DW_AT_low_pc"); break;
    }
}

uint64_t get_dw_at_high_pc(const AbbrevEntry &entry, const AttrBuffer &buffer) {
    switch(entry.form) {
    case DW_FORM_data4: {
        int64_t temp = 0;
        return read_u32(buffer.buffer.get(), temp);
        break;
    }
    case DW_FORM_data8: {
        int64_t temp = 0;
        return read_u64(buffer.buffer.get(), temp);
        break;
    }
    default: FIG_PANIC("unsuporrted Form for DW_AT_high_pc"); break;
    }
}