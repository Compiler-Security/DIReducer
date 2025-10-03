#include "DWARF/AbbrevParser.h"
#include <string.h>                   // for strlen
#include <iostream>                   // for basic_ostream, operator<<, char...
#include <string>                     // for operator<<
#include "DWARF/AbbrevEntry.h"       // for abbrev_entry
#include "DWARF/CompilationUnit.h"   // for compilation_unit
#include "DWARF/DWARFExprParser.h"  // for dwarf_expr_parser
#include "Fig/panic.hpp"              // for FIG_PANIC
#include "Utils/Buffer.h"          // for read_u32, read_u8, read_u64
#include "libdwarf/dwarf.h"           // for DW_FORM_addr, DW_FORM_addrx

AbbrevParser::AbbrevParser(CompilationUnit *cu) {
    this->addressSize = cu->addressSize;
    this->bitWidth    = cu->bitWidth;
    this->cuOffset    = cu->offset;
}

uint32_t AbbrevParser::__get_length_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const {
    int originOffset = offset;
    switch(abbrevEntry.form) {
    case DW_FORM_flag_present:
    case DW_FORM_implicit_const: {
        return 0;
    }
    case DW_FORM_data1:
    case DW_FORM_flag:
    case DW_FORM_strx1:
    case DW_FORM_ref1:
    case DW_FORM_addrx1: {
        return 1;
    }
    case DW_FORM_data2:
    case DW_FORM_ref2:
    case DW_FORM_strx2:
    case DW_FORM_addrx2: {
        return 2;
    }
    case DW_FORM_strx3:
    case DW_FORM_addrx3: {
        return 3;
    }
    case DW_FORM_strx4:
    case DW_FORM_addrx4:
    case DW_FORM_data4:
    case DW_FORM_ref_sup4:
    case DW_FORM_ref4: {
        return 4;
    }
    case DW_FORM_ref8:
    case DW_FORM_ref_sig8:
    case DW_FORM_data8:
    case DW_FORM_ref_sup8: {
        return 8;
    }
    case DW_FORM_data16: {
        return 16;
    }
    case DW_FORM_strp:
    case DW_FORM_ref_addr:
    case DW_FORM_sec_offset:
    case DW_FORM_strp_sup:
    case DW_FORM_line_strp: {
        return (this->bitWidth == 64 ? 8 : 4);
    }
    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_strx:
    case DW_FORM_addrx:
    case DW_FORM_ref_udata:
    case DW_FORM_loclistx:
    case DW_FORM_rnglistx: {
        return __get_length_leb128(buffer, offset);
    }
    case DW_FORM_block:
    case DW_FORM_exprloc: {
        return read_leb128u(buffer, offset) + offset - originOffset;
    }
    case DW_FORM_addr: {
        return this->addressSize;
    }
    case DW_FORM_string: {
        return strlen(buffer + offset) + 1;
    }
    case DW_FORM_block1: {
        return read_u8(buffer, offset) + 1;
    }
    case DW_FORM_block2: {
        return read_u16(buffer, offset) + 2;
    }
    case DW_FORM_block4: {
        return read_u32(buffer, offset) + 4;
    }
    case DW_FORM_indirect: {
        AbbrevEntry next  = abbrevEntry;
        next.form          = read_leb128u(buffer, offset); 
        uint32_t leb_lengh = offset - originOffset;
        return leb_lengh + __get_length_abbrev_entry(next, buffer, offset);
    }
    default: FIG_PANIC("Error in parsing DW_FORM", "  value: ", abbrevEntry.form);
    }
}

uint32_t AbbrevParser::get_length_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const {
    int64_t originOffset = offset;
    uint32_t result      = this->__get_length_abbrev_entry(abbrevEntry, buffer, offset);
    offset               = originOffset;
    return result;
}

void AbbrevParser::skip_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const {
    int abbrev_form_length = get_length_abbrev_entry(abbrevEntry, buffer, offset);
    offset += abbrev_form_length;
}

void AbbrevParser::print_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer) const {
    int64_t offset = 0;
    this->print_abbrev_entry(abbrevEntry, buffer, offset);
}

void AbbrevParser::print_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const {
    std::cout << std::hex;
    switch(abbrevEntry.form) {
    case DW_FORM_addr: {
        if(this->bitWidth == 64) {
            std::cout << "0x" << read_u64(buffer, offset);
        } else if(this->bitWidth == 32) {
            std::cout << "0x" << read_u32(buffer, offset);
        } else {
            FIG_PANIC("Unsupported bitWidth: ", static_cast<uint16_t>(this->bitWidth));
        }
        break;
    }
    case DW_FORM_strp: {
        uint64_t strOffset = 0;
        if(this->bitWidth == 64) {
            strOffset = read_u64(buffer, offset);
        } else if(this->bitWidth == 32) {
            strOffset = read_u32(buffer, offset);
        } else {
            FIG_PANIC("Unsupported bitWidth: ", static_cast<uint16_t>(this->bitWidth));
        }
        std::cout << "(indirect string, offset: 0x" << strOffset << ")";
        break;
    }
    case DW_FORM_ref_addr: {
        uint64_t refOffset = 0;
        if(this->bitWidth == 64) {
            refOffset = read_u64(buffer, offset);
        } else if(this->bitWidth == 32) {
            refOffset = read_u32(buffer, offset);
        } else {
            FIG_PANIC("Unsupported bitWidth: ", static_cast<uint16_t>(this->bitWidth));
        }
        std::cout << "<0x" << refOffset << ">";
        break;
    }
    case DW_FORM_sec_offset: {
        uint64_t secOffset = 0;
        if(this->bitWidth == 64) {
            secOffset = read_u64(buffer, offset);
        } else if(this->bitWidth == 32) {
            secOffset = read_u32(buffer, offset);
        } else {
            FIG_PANIC("Unsupported bitWidth: ", static_cast<uint16_t>(this->bitWidth));
        }
        std::cout << "<0x" << secOffset << ">";
        break;
    }
    case DW_FORM_strx: {
        uint64_t strIndex = 0;
        if(this->bitWidth == 64) {
            strIndex = read_u64(buffer, offset);
        } else if(this->bitWidth == 32) {
            strIndex = read_u32(buffer, offset);
        } else {
            FIG_PANIC("Unsupported bitWidth: ", static_cast<uint16_t>(this->bitWidth));
        }
        std::cout << "(indirect string, index: " << strIndex << ")";
        break;
    }
    case DW_FORM_strp_sup: {
        uint64_t strOffset = 0;
        if(this->bitWidth == 64) {
            strOffset = read_u64(buffer, offset);
        } else if(this->bitWidth == 32) {
            strOffset = read_u32(buffer, offset);
        } else {
            FIG_PANIC("Unsupported bitWidth: ", static_cast<uint16_t>(this->bitWidth));
        }
        std::cout << "(indirect string, offset: 0x" << strOffset << ")";
        break;
    }
    case DW_FORM_line_strp: {
        uint64_t strOffset = 0;
        if(this->bitWidth == 64) {
            strOffset = read_u64(buffer, offset);
        } else if(this->bitWidth == 32) {
            strOffset = read_u32(buffer, offset);
        } else {
            FIG_PANIC("Unsupported bitWidth: ", static_cast<uint16_t>(this->bitWidth));
        }
        std::cout << "(indirect string, offset: 0x" << strOffset << ")";
        break;
    }
    case DW_FORM_exprloc: {
        uint64_t blockSize = read_leb128u(buffer, offset);
        int64_t exprOffset = offset;
        std::cout << blockSize << " byte block: ";
        for(uint64_t i = 0; i < blockSize; i++) {
            std::cout << static_cast<uint16_t>(read_u8(buffer, offset)) << " ";
        }

        DWARFExprParser dwarfExprParser(buffer + exprOffset, blockSize, this->bitWidth, this->addressSize);
        std::cout << "( ";
        dwarfExprParser.print();
        std::cout << ")";
        break;
    }
    case DW_FORM_block: {
        uint64_t blockSize = read_leb128u(buffer, offset);
        std::cout << "0x" << blockSize << " byte block: ";
        for(uint64_t i = 0; i < blockSize; i++) {
            std::cout << static_cast<uint16_t>(read_u8(buffer, offset)) << " ";
        }
        break;
    }
    case DW_FORM_block1: {
        uint32_t blockSize = read_u8(buffer, offset);
        std::cout << "0x" << blockSize << " byte block: ";
        for(uint32_t i = 0; i < blockSize; i++) {
            std::cout << read_u8(buffer, offset) << " ";
        }
        break;
    }
    case DW_FORM_block2: {
        uint32_t blockSize = read_u16(buffer, offset);
        std::cout << "0x" << blockSize << " byte block: ";
        for(uint32_t i = 0; i < blockSize; i++) {
            std::cout << static_cast<uint16_t>(read_u8(buffer, offset)) << " ";
        }
        break;
    }
    case DW_FORM_block4: {
        uint32_t blockSize = read_u32(buffer, offset);
        std::cout << "0x" << blockSize << " byte block: ";
        for(uint32_t i = 0; i < blockSize; i++) {
            std::cout << static_cast<uint16_t>(read_u8(buffer, offset)) << " ";
        }
        break;
    }
    case DW_FORM_data16: {
        std::cout << "16 byte data: ";
        for(uint32_t i = 0; i < 16; i++) {
            std::cout << static_cast<uint16_t>(read_u8(buffer, offset)) << " ";
        }
        break;
    }
    case DW_FORM_indirect: {
        AbbrevEntry next = abbrevEntry;
        next.form         = read_leb128u(buffer, offset);
        print_abbrev_entry(next, buffer, offset);
        break;
    }
    case DW_FORM_data1: {
        std::cout << "0x" << static_cast<uint16_t>(read_u8(buffer, offset));
        break;
    }
    case DW_FORM_data2: {
        std::cout << "0x" << read_u16(buffer, offset);
        break;
    }
    case DW_FORM_data4: {
        std::cout << "0x" << read_u32(buffer, offset);
        break;
    }
    case DW_FORM_data8: {
        std::cout << "0x" << read_u64(buffer, offset);
        break;
    }
    case DW_FORM_string: {
        std::cout << read_string(buffer, offset);
        break;
    }
    case DW_FORM_flag: {
        std::cout << static_cast<uint16_t>(read_u8(buffer, offset));
        break;
    }
    case DW_FORM_sdata: {
        std::cout << read_leb128s(buffer, offset);
        break;
    }
    case DW_FORM_udata: {
        std::cout << read_leb128u(buffer, offset);
        break;
    }
    case DW_FORM_ref1: {
        std::cout << "<0x" << this->cuOffset + read_u8(buffer, offset) << ">";
        break;
    }
    case DW_FORM_ref2: {
        std::cout << "<0x" << this->cuOffset + read_u16(buffer, offset) << ">";
        break;
    }
    case DW_FORM_ref4: {
        std::cout << "<0x" << this->cuOffset + read_u32(buffer, offset) << ">";
        break;
    }
    case DW_FORM_ref8: {
        std::cout << "<0x" << this->cuOffset + read_u64(buffer, offset) << ">";
        break;
    }
    case DW_FORM_ref_udata: {
        std::cout << "<0x" << this->cuOffset + read_leb128u(buffer, offset) << ">";
        break;
    }
    case DW_FORM_flag_present: {
        std::cout << 1;
        break;
    }
    case DW_FORM_addrx: {
        std::cout << "(indirect addr, index: " << read_leb128u(buffer, offset) << ")";
        break;
    }
    case DW_FORM_ref_sup4: {
        std::cout << "(sup <" << read_u32(buffer, offset) << ">)";
        break;
    }
    case DW_FORM_ref_sig8: {
        std::cout << "(signature: " << read_u64(buffer, offset) << ")";
        break;
    }
    case DW_FORM_implicit_const: {
        std::cout << abbrevEntry.implicit_const;
        break;
    }
    case DW_FORM_loclistx: {
        std::cout << "<0x" << read_leb128u(buffer, offset) << ">";
        break;
    }
    case DW_FORM_rnglistx: {
        std::cout << "<0x" << read_leb128u(buffer, offset) << ">";
        break;
    }
    case DW_FORM_ref_sup8: {
        std::cout << "(sup <" << read_u64(buffer, offset) << ">)";
        break;
    }
    case DW_FORM_strx1: {
        std::cout << "(indirect string, index: " << static_cast<uint16_t>(read_u8(buffer, offset)) << ")";
        break;
    }
    case DW_FORM_strx2: {
        std::cout << "(indirect string, index: " << read_u16(buffer, offset) << ")";
        break;
    }
    case DW_FORM_strx3: {
        std::cout << "(indirect string, index: " << read_u24(buffer, offset) << ")";
        break;
    }
    case DW_FORM_strx4: {
        std::cout << "(indirect string, index: " << read_u32(buffer, offset) << ")";
        break;
    }
    case DW_FORM_addrx1: {
        std::cout << "(indirect addr, index: " << static_cast<uint16_t>(read_u8(buffer, offset)) << ")";
        break;
    }
    case DW_FORM_addrx2: {
        std::cout << "(indirect addr, index: " << read_u16(buffer, offset) << ")";
        break;
    }
    case DW_FORM_addrx3: {
        std::cout << "(indirect addr, index: " << read_u24(buffer, offset) << ")";
        break;
    }
    case DW_FORM_addrx4: {
        std::cout << "(indirect addr, index: " << read_u32(buffer, offset) << ")";
        break;
    }
    default: {
        FIG_PANIC("Error in parsing DW_FORM", "  value: ", abbrevEntry.form);
    }
    }
    std::cout << std::dec;
}