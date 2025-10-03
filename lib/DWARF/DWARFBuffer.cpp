#include "DWARF/DWARFBuffer.h"
#include <iostream>           // for basic_ostream, operator<<, char_traits
#include <vector>             // for vector
#include "DWARF/ELFObject.h" // for elf_object
#include "Fig/panic.hpp"

DWARFBuffer::DWARFBuffer() :
    elfBuffer(nullptr) {
}

DWARFBuffer::DWARFBuffer(ELFObjetc &elfObject) {
    this->init(elfObject);
}

void DWARFBuffer::init(ELFObjetc &elfObject) {
    this->elfBuffer                   = elfObject.buffer;
    std::vector<dr_Elf64_Shdr> &shdrs = elfObject.shdrs;
    if(elfObject.is_section_exist(".debug_info")) {
        int index = elfObject.get_section_index_by_name(".debug_info");
        this->infoBuffer.set(this->elfBuffer.get() + shdrs[index].shdr.sh_offset);
        this->infoSize.set(shdrs[index].shdr.sh_size);
    } else {
        FIG_PANIC("can not find section .debug_info");
    }
    if(elfObject.is_section_exist(".debug_abbrev")) {
        int index = elfObject.get_section_index_by_name(".debug_abbrev");
        this->abbrevBuffer.set(this->elfBuffer.get() + shdrs[index].shdr.sh_offset);
        this->abbrevSize.set(shdrs[index].shdr.sh_size);
    } else {
        FIG_PANIC("can not find section .debug_abbrev");
    }

    if(elfObject.is_section_exist(".debug_line")) {
        int index = elfObject.get_section_index_by_name(".debug_line");
        this->lineBuffer.set(this->elfBuffer.get() + shdrs[index].shdr.sh_offset);
        this->lineSize.set(shdrs[index].shdr.sh_size);
    }
    if(elfObject.is_section_exist(".debug_line_str")) {
        int index = elfObject.get_section_index_by_name(".debug_line_str");
        this->lineStrBuffer.set(this->elfBuffer.get() + shdrs[index].shdr.sh_offset);
        this->lineStrSize.set(shdrs[index].shdr.sh_size);
    }
    if(elfObject.is_section_exist(".debug_loclists")) {
        int index = elfObject.get_section_index_by_name(".debug_loclists");
        this->loclistsBuffer.set(this->elfBuffer.get() + shdrs[index].shdr.sh_offset);
        this->loclistsSize.set(shdrs[index].shdr.sh_size);
    }
}

void DWARFBuffer::print() {
    std::cout << "***** DWARF BUFFET INFORMATION *****" << std::endl;
    std::cout << "info buffer: " << this->infoBuffer.get() << std::endl;
    std::cout << "info size: " << this->infoSize.get() << std::endl;
    std::cout << "abbrev buffer: " << this->abbrevBuffer.get() << std::endl;
    std::cout << "abbrev size: " << this->abbrevSize.get() << std::endl;
    if(this->lineBuffer.is_set()) {
        std::cout << "line buffer: " << this->lineBuffer.get() << std::endl;
        std::cout << "line size: " << this->lineSize.get() << std::endl;
    }
    if(this->loclistsBuffer.is_set()) {
        std::cout << "loclists buffer: " << this->loclistsBuffer.get() << std::endl;
        std::cout << "loclists size: " << this->loclistsSize.get() << std::endl;
    }
    std::cout << "***** DWARF BUFFET INFORMATION *****" << std::endl;
}