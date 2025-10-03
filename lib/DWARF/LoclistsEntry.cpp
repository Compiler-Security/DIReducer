#include "DWARF/LoclistsEntry.h"
#include <iostream>           // for char_traits, basic_ostream, operator<<
#include "Fig/panic.hpp"      // for FIG_PANIC
#include "Utils/Buffer.h"  // for read_leb128u, read_u32, read_u64, read_u8
#include "Utils/ToStr.h"     // for dw_lle_to_str
#include "libdwarf/dwarf.h"   // for DW_LLE_base_address, DW_LLE_base_addressx

void LoclistsEntry::reset() {
    this->offset    = 0;
    this->type      = 0;
    this->operand1  = 0;
    this->operand2  = 0;
    this->blockSize = 0;
}

LoclistsEntry::LoclistsEntry(const char *buffer, int64_t &offset, uint8_t addressSize, uint8_t bitWidth) {
    this->init(buffer, offset, addressSize, bitWidth);
}

void LoclistsEntry::print() {
    std::cout << std::hex << "<0x" << this->offset << "> " << dw_lle_to_str(this->type) << std::endl;
}

void LoclistsEntry::init(const char *buffer, int64_t &offset, uint8_t addressSize, [[maybe_unused]] uint8_t bitWidth) {
    this->reset();
    this->offset = offset;
    this->type   = read_u8(buffer, offset);
    switch(this->type) {
    case DW_LLE_end_of_list: {
        /* DO NOTHING */
        break;
    }
    case DW_LLE_base_addressx: {
        this->operand1 = read_leb128u(buffer, offset);
        break;
    }
    case DW_LLE_startx_endx:
    case DW_LLE_startx_length:
    case DW_LLE_offset_pair: {
        this->operand1    = read_leb128u(buffer, offset);
        this->operand2    = read_leb128u(buffer, offset);
        this->blockSize   = read_leb128u(buffer, offset);
        this->blockBuffer = const_cast<char *>(buffer) + offset;
        offset += this->blockSize;
        break;
    }
    case DW_LLE_default_location: {
        this->blockSize   = read_leb128u(buffer, offset);
        this->blockBuffer = const_cast<char *>(buffer) + offset;
        offset += this->blockSize;
        break;
    }
    case DW_LLE_base_address: {
        if(addressSize == 8) {
            this->address1 = read_u64(buffer, offset);
        } else {
            this->address1 = read_u32(buffer, offset);
        }
        break;
    }
    case DW_LLE_start_end: {
        if(addressSize == 8) {
            this->address1 = read_u64(buffer, offset);
            this->address2 = read_u64(buffer, offset);
        } else {
            this->address1 = read_u32(buffer, offset);
            this->address2 = read_u32(buffer, offset);
        }
        this->blockSize   = read_leb128u(buffer, offset);
        this->blockBuffer = const_cast<char *>(buffer) + offset;
        offset += this->blockSize;
        break;
    }
    case DW_LLE_start_length: {
        if(addressSize == 8) {
            this->address1 = read_u64(buffer, offset);
        } else {
            this->address1 = read_u32(buffer, offset);
        }
        this->operand2    = read_leb128u(buffer, offset);
        this->blockSize   = read_leb128u(buffer, offset);
        this->blockBuffer = const_cast<char *>(buffer) + offset;
        offset += this->blockSize;
        break;
    }
    default: FIG_PANIC("Unsupported DW_LLE, value: ", static_cast<uint16_t>(this->type)); break;
    }
}