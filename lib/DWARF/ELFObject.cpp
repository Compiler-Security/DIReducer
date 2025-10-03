#include <algorithm>             // for find, max
#include <cstdint>               // for int64_t, uint64_t
#include <cstring>               // for memcpy, size_t
#include <fstream>               // for basic_ostream, operator<<, endl
#include <iomanip>               // for operator<<, setfill, setw
#include <iostream>              // for cout
#include <memory>                // for shared_ptr, allocator, uniqu...
#include <string>                // for char_traits, string, operator==
#include <vector>                // for vector
#include "DWARF/ELFObject.h"     // for ELFObjetc, StrTable
#include "Fig/panic.hpp"         // for FIG_PANIC
#include "Utils/Common.h"        // for dec_to_hex
#include "Utils/Struct.h"        // for dr_Elf64_Shdr
#include "libelf/elf.h"          // for (anonymous), Elf64_Ehdr, Elf...
#include "nlohmann/json_fwd.hpp" // for json

ELFObjetc::ELFObjetc(const std::string &path) {
    this->init(path);
}

bool ELFObjetc::is_section_exist(const std::string &name) {
    for(auto &shdr : this->shdrs) {
        if(shdr.name == name) {
            return true;
        }
    }

    return false;
}

int64_t ELFObjetc::get_section_index_by_name(const std::string &name) const {
    for(uint64_t i = 0; i < this->shdrs.size(); i++) {
        auto &shdr = this->shdrs[i];
        if(shdr.name == name) {
            return i;
        }
    }

    FIG_PANIC("Section ", name, " not found");
}

dr_Elf64_Shdr &ELFObjetc::getSecByName(const std::string &name) {
    return this->shdrs[this->get_section_index_by_name(name)];
}

void ELFObjetc::print() {
    this->print_ehdr();
    this->print_phdrs();
    this->print_shdrs();
    this->print_mapping_sections();
}

void ELFObjetc::print_ehdr() {
    this->print_ehdr(this->ehdr);
}

void ELFObjetc::print_ehdr(Elf64_Ehdr &ehdr) {
    std::cout << "****************" << std::endl;
    std::cout << "*  ELF Header  *" << std::endl;
    std::cout << "****************" << std::endl;
    std::cout << "Magic:  " << std::hex;
    for(int i = 0; i < EI_NIDENT; i++) {
        std::cout << " " << std::setfill('0') << std::setw(2) << (unsigned int)ehdr.e_ident[i] << " ";
    }
    std::cout << std::setfill('0') << std::setw(0) << std::endl;
    std::cout << "e_type:\t\t\t0x" << ehdr.e_type << std::endl;
    std::cout << "e_machine:\t\t0x" << ehdr.e_machine << std::endl;
    std::cout << "e_version:\t\t0x" << ehdr.e_version << std::endl;
    std::cout << "e_entry:\t\t0x" << ehdr.e_entry << std::endl;
    std::cout << std::dec;
    std::cout << "e_phoff:\t\t" << ehdr.e_phoff << std::endl;
    std::cout << "e_shoff:\t\t" << ehdr.e_shoff << std::endl;
    std::cout << std::setfill('0') << std::setw(0) << std::hex;
    std::cout << "e_flags:\t\t0x" << ehdr.e_flags << std::endl;
    std::cout << std::dec;
    std::cout << "e_ehsize:\t\t" << ehdr.e_ehsize << " (bytes)" << std::endl;
    std::cout << "e_phentsize:\t" << ehdr.e_phentsize << " (bytes)" << std::endl;
    std::cout << "e_phnum:\t\t" << ehdr.e_phnum << std::endl;
    std::cout << "e_shentsize:\t" << ehdr.e_shentsize << " (bytes)" << std::endl;
    std::cout << "e_shnum:\t\t" << ehdr.e_shnum << std::endl;
    std::cout << "e_shstrndx:\t\t" << ehdr.e_shstrndx << std::endl;
    std::cout << "****************" << std::endl;
    std::cout << std::dec << std::endl;
}

void ELFObjetc::print_phdrs() {
    this->print_phdrs(this->phdrs);
}

void ELFObjetc::print_phdrs(std::vector<Elf64_Phdr> &phdrs) {
    std::cout << "********************" << std::endl;
    std::cout << "*  program header  *" << std::hex << std::endl;
    std::cout << "********************" << std::endl;
    for(auto &it : phdrs) {
        std::cout << "----------------" << std::endl;
        std::cout << "p_type:  0x" << it.p_type << std::endl;
        std::cout << "p_flags:  0x" << it.p_flags << std::endl;
        std::cout << "p_offset:  0x" << it.p_offset << std::endl;
        std::cout << "p_vaddr:  0x" << it.p_vaddr << std::endl;
        std::cout << "p_paddr:  0x" << it.p_paddr << std::endl;
        std::cout << "p_filesz:  0x" << it.p_filesz << std::endl;
        std::cout << "p_memsz:  0x" << it.p_memsz << std::endl;
        std::cout << "p_align:  0x" << it.p_align << std::endl;
        std::cout << "----------------" << std::endl;
    }
    std::cout << "****************" << std::dec << std::endl
              << std::endl;
}

void ELFObjetc::print_shdrs() {
    this->print_shdrs(this->shdrs);
}

void ELFObjetc::print_shdrs(std::vector<dr_Elf64_Shdr> &shdrs) {
    std::cout << "******************" << std::endl;
    std::cout << "* section header *" << std::hex << std::endl;
    std::cout << "******************" << std::endl;
    for(auto &it : shdrs) {
        std::cout << "----------------" << std::endl;
        std::cout << "name(string):  " << it.name << std::endl;
        std::cout << "sh_name:  0x" << it.shdr.sh_name << std::endl;
        std::cout << "sh_type:  0x" << it.shdr.sh_type << std::endl;
        std::cout << "sh_flags:  0x" << it.shdr.sh_flags << std::endl;
        std::cout << "sh_addr:  0x" << it.shdr.sh_addr << std::endl;
        std::cout << "sh_offset:  0x" << it.shdr.sh_offset << std::endl;
        std::cout << "sh_size:  0x" << it.shdr.sh_size << std::endl;
        std::cout << "sh_link:  0x" << it.shdr.sh_link << std::endl;
        std::cout << "sh_info:  0x" << it.shdr.sh_info << std::endl;
        std::cout << "sh_addralign:  0x" << it.shdr.sh_addralign << std::endl;
        std::cout << "sh_entsize:  0x" << it.shdr.sh_entsize << std::endl;
        std::cout << "----------------" << std::endl;
    }
    std::cout << "****************" << std::dec << std::endl
              << std::endl;
}

void ELFObjetc::init(const std::string &path) {
    this->filePath = path;
    this->init_buffer(path);
    this->init_elf_header();
    this->init_section_headers();
    this->init_program_headers();
    this->init_section_mapping_segment();
}

void ELFObjetc::init_buffer(const std::string &path) {
    // 读取文件到缓冲区当中
    std::ifstream file(path);
    if(!file.is_open())
        FIG_PANIC("Error occurs in open file ", path);

    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    this->size              = fileSize;
    file.seekg(0, std::ios::beg);
    this->buffer.reset(new char[fileSize]);
    file.read(this->buffer.get(), fileSize);
    file.close();
}

void ELFObjetc::init_elf_header() {
    // 读取ELF Header
    std::memcpy(&this->ehdr, this->buffer.get(), sizeof(Elf64_Ehdr));
    if(this->ehdr.e_ident[0] != 0x7f || this->ehdr.e_ident[1] != 'E' || this->ehdr.e_ident[2] != 'L' || this->ehdr.e_ident[3] != 'F') {
        FIG_PANIC("Incorrect ELF format, magic number is invalid.");
    }
}

void ELFObjetc::init_program_headers() {
    this->phdrs.resize(ehdr.e_phnum);
    std::memcpy(this->phdrs.data(), buffer.get() + ehdr.e_phoff, ehdr.e_phnum * sizeof(Elf64_Phdr));
}

void ELFObjetc::init_section_headers() {
    Elf64_Ehdr &ehdr = this->ehdr;
    if(ehdr.e_shnum == 0 || ehdr.e_shoff == 0) {
        FIG_PANIC("There are no sections in this elf file");
    }
    std::unique_ptr<Elf64_Shdr[]> shdrPointer(new Elf64_Shdr[ehdr.e_shnum]);
    std::memcpy(shdrPointer.get(), buffer.get() + ehdr.e_shoff, ehdr.e_shnum * sizeof(Elf64_Shdr));
    this->shdrs.resize(ehdr.e_shnum);
    for(int i = 0; i < ehdr.e_shnum; i++) {
        this->shdrs[i].shdr = shdrPointer[i];
    }
    shdrPointer.reset();
    int shstrtab_offset = this->shdrs[ehdr.e_shstrndx].shdr.sh_offset;
    for(auto &shdr : this->shdrs) {
        shdr.name.assign(this->buffer.get() + shstrtab_offset + shdr.shdr.sh_name);
    }
    const auto &textShdr = this->getSecByName(".text");
    this->baseAddr       = textShdr.shdr.sh_addr - textShdr.shdr.sh_offset;
}

void ELFObjetc::init_section_mapping_segment() {
    for(auto &seg : this->phdrs) {
        int64_t seg_start = seg.p_offset;
        int64_t seg_end   = seg_start + seg.p_filesz;
        this->section_mapping_segment.push_back(std::vector<std::string>());
        for(auto &sec : this->shdrs) {
            if(sec.name == "")
                continue;
            int64_t sec_start = sec.shdr.sh_offset;
            int64_t sec_end   = sec_start + sec.shdr.sh_size;
            if(seg_start <= sec_start && seg_end >= sec_end) {
                this->section_mapping_segment.back().push_back(sec.name);
            }
        }
    }
}

void ELFObjetc::print_mapping_sections() {
    for(size_t i = 0; i < this->phdrs.size(); i++) {
        std::cout << "program header [" << i << "]: ";
        for(auto &it : this->section_mapping_segment[i]) {
            std::cout << it << " ";
        }
        std::cout << std::endl;
    }
}

int64_t ELFObjetc::get_mapping_end() const {
    int64_t mapping_end = 0;
    for(auto &it : this->section_mapping_segment) {
        if(it.size() == 0)
            continue;

        int64_t section_index    = this->get_section_index_by_name(it.back());
        const dr_Elf64_Shdr &sec = this->shdrs[section_index];
        int64_t sec_end          = sec.shdr.sh_offset + sec.shdr.sh_size;
        mapping_end              = std::max(mapping_end, sec_end);
    }

    return mapping_end;
}

int64_t ELFObjetc::getDwarfSize() {
    int64_t size                   = 0;
    const std::string dwarf_prefix = ".debug";
    for(auto &s : this->shdrs) {
        if(s.name.substr(0, dwarf_prefix.length()) != dwarf_prefix)
            continue;
        size += s.shdr.sh_size;
    }

    return size;
}

StrTable::StrTable() :
    strBuffer(nullptr), strSize(0) {
}

StrTable::StrTable(char *buffer, uint64_t size) {
    this->init(buffer, size);
}

void StrTable::init(char *buffer, uint64_t size) {
    this->strBuffer = buffer;
    this->strSize   = size;
}

bool StrTable::isInitialized() {
    return this->strBuffer != nullptr;
}

std::string StrTable::get(uint64_t offset) const {
    char *limit  = this->strBuffer + this->strSize;
    char *strEnd = std::find(this->strBuffer + offset, limit, '\0');
    return std::string(this->strBuffer + offset, strEnd);
}

nlohmann::json StrTable::toJson(bool withHeader) {
    nlohmann::json content;
    char *limit   = this->strBuffer + this->strSize;
    char *current = this->strBuffer;
    while(current < limit) {
        std::string keyStr = "0x" + dec_to_hex(static_cast<uint64_t>(current - this->strBuffer));
        content[keyStr]    = std::string(current);
        current            = std::find(current, limit, '\0') + 1;
    }

    if(withHeader)
        return {{"StrTable", content}};
    else
        return content;
}