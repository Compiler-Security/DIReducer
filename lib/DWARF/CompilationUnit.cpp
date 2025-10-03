#include <libdwarf/dwarf.h>        // for DW_UT_skeleton, DW_UT_split_compile
#include <stdint.h>                // for int64_t, uint64_t, uint16_t
#include <iostream>                // for basic_ostream, char_traits, oper...
#include <iterator>                // for next
#include <memory>                  // for shared_ptr, __shared_ptr_access
#include <unordered_map>           // for unordered_map
#include <unordered_set>           // for unordered_set
#include <vector>                  // for vector
#include "DWARF/AbbrevEntry.h"     // for abbrev_entry
#include "DWARF/AbbrevItem.h"      // for abbrev_item
#include "DWARF/AbbrevObject.h"    // for abbrev_object
#include "DWARF/AbbrevParser.h"    // for abbrev_parser
#include "DWARF/AttrBuffer.h"      // for attr_buffer
#include "DWARF/CompilationUnit.h" // for compilation_unit
#include "DWARF/InfoEntry.h"       // for info_entry
#include "Utils/Buffer.h"          // for read_u64, write_u64, read_u32
#include "Fig/panic.hpp"
#include "Utils/ToStr.h"  // for dw_ut_to_str
#include "Utils/Struct.h" // for dr_immut
struct LoclistsObject;

CompilationUnit::CompilationUnit(const char *const buffer, int64_t &offset, const std::shared_ptr<AbbrevObject> &abbrevObjectRef, uint16_t module) {
    this->offset = offset;
    this->module = module;
    this->infoBuffer.set(buffer);
    this->init_unit_header(buffer, offset);
    this->abbrevList = abbrevObjectRef->getAbbrevList(this->abbrevOffset);
    this->abbrevParser.reset(new AbbrevParser(this));
    this->infoEntry.init(buffer, offset, this->abbrevParser, this->abbrevList, 0, this->module);
    this->loclists_base    = 0;
    auto rootEntryIterator = this->infoEntry.abbrevItem.entries.begin();
    auto rootAttrIterator  = this->infoEntry.attrBuffers.begin();
    while(!rootEntryIterator->isNull()) {
        if(rootEntryIterator->at == DW_AT_loclists_base && rootEntryIterator->form == DW_FORM_sec_offset) {
            int64_t tmpOffset = 0;
            if(this->bitWidth == 64)
                this->loclists_base = read_u64(rootAttrIterator->buffer.get(), tmpOffset);
            else
                this->loclists_base = read_u32(rootAttrIterator->buffer.get(), tmpOffset);
            break;
        }
        rootEntryIterator = std::next(rootEntryIterator);
        rootAttrIterator  = std::next(rootAttrIterator);
    }

    offset = this->offset + this->length + (this->bitWidth == 64 ? 12 : 4);
}

int CompilationUnit::get_unit_header_size() const {
    if(this->bitWidth == 32 && this->version == 4) {
        return 11;
    } else if(this->bitWidth == 32 && this->version == 5) {
        int res = 12;
        if(this->unit_type == DW_UT_skeleton || this->unit_type == DW_UT_split_compile || this->unit_type == DW_UT_split_type) {
            res += 8;
        } else if(this->unit_type == DW_UT_type) {
            res += 16;
        }
        return res;
    } else if(this->bitWidth == 64 && this->version == 4) {
        return 23;
    } else if(this->bitWidth == 64 && this->version == 5) {
        int res = 24;
        if(this->unit_type == DW_UT_skeleton || this->unit_type == DW_UT_split_compile || this->unit_type == DW_UT_split_type) {
            res += 8;
        } else if(this->unit_type == DW_UT_type) {
            res += 16;
        }
        return res;
    } else {
        FIG_PANIC("Unsupported bitwidth or version");
    }
}

int CompilationUnit::getLengthSize() {
    if(this->bitWidth == 32) {
        return 4;
    } else if(this->bitWidth == 64) {
        return 12;
    } else {
        FIG_PANIC("Unsupported bitwidth");
    }
}

int64_t CompilationUnit::getSize() {
    return this->get_unit_header_size() + this->infoEntry.getSize();
}

void CompilationUnit::fix_offset(
    int64_t &curCUOffset,
    std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    std::unordered_map<int64_t, int64_t> &DIEOffsetMap) {
    this->offset                 = curCUOffset;
    this->abbrevParser->cuOffset = curCUOffset;
    int64_t innerOffset          = 0;
    innerOffset += this->get_unit_header_size();
    this->infoEntry.fix_offset(curCUOffset, innerOffset, GlobalOffsetMap, DIEOffsetMap);
}

void CompilationUnit::fix_reference_die_loclists(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    std::unordered_set<uint64_t> &loclistOffsetFixedSet,
    std::shared_ptr<LoclistsObject> &loclistsObject) {
    this->infoEntry.fix_reference_die_loclists(
        GlobalOffsetMap, DIEOffsetMap, loclistOffsetFixedSet, loclistsObject, this->loclists_base);
}

void CompilationUnit::fix_reference_die_attr(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
    const std::unordered_map<uint64_t, uint64_t> &CUOffsetMap) {
    this->infoEntry.fix_reference_die_attr(GlobalOffsetMap, DIEOffsetMap, CUOffsetMap);
}

void CompilationUnit::init_unit_header(const char *const buffer, int64_t &offset) {
    this->length = read_u32(buffer, offset);
    // 判断64/32 bit的保留数字 0xffffffff
    if(this->length == 0xffffffff) {
        this->length   = read_u64(buffer, offset);
        this->bitWidth = 64;
    } else {
        this->bitWidth = 32;
    }
    this->version = read_u16(buffer, offset);

    if(this->version == 4) {
        this->__init_unit_header_dwarf_4(buffer, offset);
    } else if(this->version == 5) {
        this->__init_unit_header_dwarf_5(buffer, offset);
    } else {
        FIG_PANIC("Unsupported DWARF version ", this->version);
    }
}

void CompilationUnit::__init_unit_header_dwarf_4(const char *const buffer, int64_t &offset) {
    this->unit_type = 0;
    if(this->bitWidth == 64) {
        this->abbrevOffset = read_u64(buffer, offset);
    } else {
        this->abbrevOffset = read_u32(buffer, offset);
    }

    this->addressSize = read_u8(buffer, offset);
}

void CompilationUnit::__init_unit_header_dwarf_5(const char *const buffer, int64_t &offset) {
    this->unit_type   = read_u8(buffer, offset);
    this->addressSize = read_u8(buffer, offset);

    if(this->bitWidth == 64) {
        this->abbrevOffset = read_u64(buffer, offset);
    } else {
        this->abbrevOffset = read_u32(buffer, offset);
    }

    if(this->unit_type == DW_UT_skeleton || this->unit_type == DW_UT_split_compile || this->unit_type == DW_UT_split_type) {
        this->dwoId = read_u64(buffer, offset);
    } else if(this->unit_type == DW_UT_type) {
        this->typeSignature = read_u64(buffer, offset);
        this->typeOffset    = read_u64(buffer, offset);
    }
}

void CompilationUnit::write(char *buffer, int64_t &offset) {
    this->write_unit_header(buffer, offset);
    this->infoEntry.write(buffer, offset);
}

void CompilationUnit::write_unit_header(char *buffer, int64_t &offset) {
    if(!(this->bitWidth == 64 || this->bitWidth == 32))
        FIG_PANIC("Unsupported bitwidth ", this->bitWidth);
    if(!(this->version == 4 || this->version == 5))
        FIG_PANIC("Unsupported version ", this->version);

    if(this->bitWidth == 64) {
        write_u32(0xffffffff, buffer, offset);
        write_u64(this->length, buffer, offset);
    } else {
        write_u32(this->length, buffer, offset);
    }

    write_u16(this->version, buffer, offset);
    if(this->version == 4) {
        if(this->bitWidth == 64) {
            write_u64(this->abbrevOffset, buffer, offset);
        } else {
            write_u32(this->abbrevOffset, buffer, offset);
        }
        write_u8(this->addressSize, buffer, offset);

    } else {
        write_u8(this->unit_type, buffer, offset);
        write_u8(this->addressSize, buffer, offset);
        if(this->bitWidth == 64) {
            write_u64(this->abbrevOffset, buffer, offset);
        } else {
            write_u32(this->abbrevOffset, buffer, offset);
        }

        if(this->unit_type == DW_UT_skeleton || this->unit_type == DW_UT_split_compile || this->unit_type == DW_UT_split_type) {
            write_u64(this->dwoId, buffer, offset);
        } else if(this->unit_type == DW_UT_type) {
            write_u64(this->typeSignature, buffer, offset);
            write_u64(this->typeOffset, buffer, offset);
        }
    }
}

void CompilationUnit::print() {
    this->print_offset();
    this->print_unit_header();
    this->infoEntry.print();
}

void CompilationUnit::print_offset() {
    std::cout << "  Compilation Unit @ offset 0x" << std::hex << this->offset << std::dec << ":" << std::endl;
}

void CompilationUnit::print_unit_header() {
    std::cout << "   Length:\t0x" << std::hex << this->length << std::dec << (this->bitWidth == 64 ? " (64-bit)" : " (32-bit)") << std::endl;
    std::cout << "   Version:\t" << this->version << std::endl;
    if(this->version == 5) {
        std::cout << "   Unit Type:\t" << dw_ut_to_str(this->unit_type) << std::endl;
    }
    std::cout << "   Abbrev Offset:\t0x" << std::hex << this->abbrevOffset << std::dec << std::endl;
    std::cout << "   Pointer Size:\t" << static_cast<uint16_t>(this->addressSize) << std::endl;
    if(this->unit_type == DW_UT_skeleton || this->unit_type == DW_UT_split_compile || this->unit_type == DW_UT_split_type) {
        std::cout << "Dwo Id:\t" << this->dwoId << std::endl;
    }
    if(this->unit_type == DW_UT_type) {
        std::cout << "Type Signature:\t" << this->typeSignature << std::endl;
        std::cout << "Type Offset:\t" << this->typeOffset << std::endl;
    }
}
