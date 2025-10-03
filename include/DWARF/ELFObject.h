#ifndef ELF_OBJECT_H
#define ELF_OBJECT_H

#include <cstdint>
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

#include "Utils/Struct.h"
#include "libelf/elf.h"
#include "nlohmann/json.hpp"

class ELFObjetc {
public:
    std::string filePath;
    std::shared_ptr<char[]> buffer;
    uint64_t size;
    uint64_t baseAddr;
    Elf64_Ehdr ehdr;
    std::vector<dr_Elf64_Shdr> shdrs;
    std::vector<Elf64_Phdr> phdrs;

public:
    std::vector<std::vector<std::string>> section_mapping_segment;

public:
    void print();
    void print_ehdr();
    void print_shdrs();
    void print_phdrs();
    void print_mapping_sections();

private:
    void print_ehdr(Elf64_Ehdr &ehdr);
    void print_shdrs(std::vector<dr_Elf64_Shdr> &shdrs);
    void print_phdrs(std::vector<Elf64_Phdr> &phdrs);

private:
    void init_buffer(const std::string &path);
    void init_elf_header();
    void init_program_headers();
    void init_section_headers();
    void init_section_mapping_segment();

public:
    void init(const std::string &path);
    bool is_section_exist(const std::string &name);
    int64_t get_section_index_by_name(const std::string &name) const;
    dr_Elf64_Shdr &getSecByName(const std::string &name);
    int64_t get_mapping_end() const;
    int64_t getDwarfSize();

public:
    ELFObjetc() = default;
    ELFObjetc(const std::string &path);

    ~ELFObjetc() = default;
};

struct StrTable {
private:
    char *strBuffer;
    uint64_t strSize;

public:
    StrTable();
    StrTable(char *buffer, uint64_t size);

    void init(char *buffer, uint64_t size);
    bool isInitialized();
    std::string get(uint64_t offset) const;
    nlohmann::json toJson(bool withHeader);
};

#endif
