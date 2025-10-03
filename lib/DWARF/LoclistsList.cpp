#include "DWARF/LoclistsList.h"
#include <iostream>          // for basic_ostream, char_traits, operator<<
#include "Utils/Buffer.h" // for read_u32, read_u64, read_u8, read_u16
#include "Fig/panic.hpp"

LoclistsList::LoclistsList(const char *buffer, int64_t &offset) {
    this->init(buffer, offset);
}

void LoclistsList::init(const char *buffer, int64_t &offset) {
    this->init_header(buffer, offset);
    this->init_offset_array(buffer, offset);
    offset = this->offset + this->length + (this->bitWidth == 64 ? 12 : 4);
}

void LoclistsList::init_header(const char *buffer, int64_t &offset) {
    this->offset = offset;
    this->length = read_u32(buffer, offset);
    if(this->length == 0xffffffff) {
        this->length   = read_u64(buffer, offset);
        this->bitWidth = 64;
    } else {
        this->bitWidth = 32;
    }
    this->version     = read_u16(buffer, offset);
    this->addressSize = read_u8(buffer, offset);
    if(this->addressSize != 8 && this->addressSize != 4)
        FIG_PANIC("Unsupported addressSize: ", this->addressSize);
    this->segmentSelectorSize = read_u8(buffer, offset);
    this->offsetEntryCount    = read_u32(buffer, offset);
}

void LoclistsList::init_offset_array(const char *buffer, int64_t &offset) {
    this->offsetArrayBase = offset;
    for(uint32_t i = 0; i < this->offsetEntryCount; i++) {
        if(this->bitWidth == 64)
            this->offsetArray.push_back(read_u64(buffer, offset));
        else
            this->offsetArray.push_back(read_u32(buffer, offset));
    }
}

void LoclistsList::print() {
    this->print_offset();
    this->print_header();
    this->print_offset_array();
}

void LoclistsList::print_offset() {
    std::cout << "   loclists List @ offset 0x" << std::hex << this->offset << std::dec << ":" << std::endl;
}

void LoclistsList::print_header() {
    std::cout << "   Length:\t\t\t0x" << std::hex << this->length << std::dec << (this->bitWidth == 64 ? " (64-bit)" : " (32-bit)") << std::endl;
    std::cout << "   Version:\t\t\t" << this->version << std::endl;
    std::cout << "   Address Size:\t\t" << static_cast<uint16_t>(this->addressSize) << std::endl;
    std::cout << "   Segment Selector Size:\t" << static_cast<uint16_t>(this->segmentSelectorSize) << std::endl;
    std::cout << "   Offset Entry Count:\t\t" << offsetEntryCount << std::endl;
}

void LoclistsList::print_offset_array() {
    uint32_t cnt = 0;
    for(auto &e : this->offsetArray) {
        std::cout << "[" << cnt++ << "]"
                  << "\t0x" << std::hex << e << std::dec << std::endl;
    }
}