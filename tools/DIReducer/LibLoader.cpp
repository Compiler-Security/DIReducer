#include "LibLoader.h"
#include <cstdint>               // for uint64_t, uint32_t, uint8_t
#include <cstring>               // for memcpy
#include <iomanip>               // for operator<<, setfill, setw
#include <sstream>               // for basic_ostream, basic_stringstream, hex
#include "Fig/assert.hpp"        // for FIG_ASSERT
#include "Fig/panic.hpp"         // for FIG_PANIC
#include "Utils/Common.h"        // for dec_to_hex
#include "capstone/capstone.h"   // for cs_insn, cs_disasm, cs_free, cs_open
#include "capstone/x86.h"        // for x86_insn
#include "libelf/elf.h"          // for (anonymous), Elf64_Rela, Elf64_Sym
#include "nlohmann/json_fwd.hpp" // for json

void PLTObject::init(char *buffer, uint64_t addr, uint64_t length) {
    csh handle;
    cs_insn *instructions;
    uint32_t numOfInsn;
    if(cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
        FIG_PANIC("Fail to disasmbly.");

    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    numOfInsn = cs_disasm(handle, reinterpret_cast<uint8_t *>(buffer), length, addr, 0, &instructions);

    // parse header
    FIG_ASSERT(numOfInsn >= 3);
    FIG_ASSERT(instructions[0].id == X86_INS_PUSH);
    FIG_ASSERT(instructions[1].id == X86_INS_JMP);
    FIG_ASSERT(instructions[2].id == X86_INS_NOP);
    this->header[0] = instructions[0];
    this->header[1] = instructions[1];
    this->header[2] = instructions[2];

    // parse entries
    FIG_ASSERT((numOfInsn - 3) % 4 == 0);
    this->entries.clear();
    uint32_t index = 3;
    while(index < numOfInsn) {
        std::array<cs_insn, 4> entry;
        entry[0] = instructions[index++];
        entry[1] = instructions[index++];
        entry[2] = instructions[index++];
        entry[3] = instructions[index++];
        this->entries.push_back(entry);
    }

    cs_free(instructions, numOfInsn);
}

nlohmann::json PLTObject::toJson() const {
    nlohmann::json headerJson = nlohmann::json::array();
    headerJson.push_back(std::to_string(this->header[0].address) + this->header[0].mnemonic + this->header[0].op_str);
    headerJson.push_back(std::to_string(this->header[1].address) + this->header[1].mnemonic + this->header[1].op_str);
    headerJson.push_back(std::to_string(this->header[2].address) + this->header[2].mnemonic + this->header[2].op_str);

    nlohmann::json entriesJson = nlohmann::json::array();
    for(auto &entry : this->entries) {
        entriesJson.push_back(std::to_string(entry[0].address) + entry[0].mnemonic + entry[0].op_str);
        entriesJson.push_back(std::to_string(entry[1].address) + entry[1].mnemonic + entry[1].op_str);
        entriesJson.push_back(std::to_string(entry[2].address) + entry[2].mnemonic + entry[2].op_str);
        entriesJson.push_back(std::to_string(entry[3].address) + entry[3].mnemonic + entry[3].op_str);
    }

    nlohmann::json content;
    content["header"]  = headerJson;
    content["entries"] = entriesJson;

    return content;
}

void PLTSECObject::init(char *buffer, uint64_t addr, uint64_t length) {
    csh handle;
    cs_insn *instructions;
    uint32_t numOfInsn;
    if(cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
        FIG_PANIC("Fail to disasmbly.");

    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    numOfInsn = cs_disasm(handle, reinterpret_cast<uint8_t *>(buffer), length, addr, 0, &instructions);
    // parse entries
    FIG_ASSERT(numOfInsn % 3 == 0);
    this->entries.clear();
    uint32_t index = 0;
    while(index < numOfInsn) {
        std::array<cs_insn, 3> entry;
        entry[0] = instructions[index++];
        entry[1] = instructions[index++];
        entry[2] = instructions[index++];
        this->entries.push_back(entry);
    }

    cs_free(instructions, numOfInsn);
}

nlohmann::json PLTSECObject::toJson() const {
    nlohmann::json content = nlohmann::json::array();
    for(auto &entry : this->entries) {
        content.push_back(std::to_string(entry[0].address) + entry[0].mnemonic + entry[0].op_str);
        content.push_back(std::to_string(entry[1].address) + entry[1].mnemonic + entry[1].op_str);
        content.push_back(std::to_string(entry[2].address) + entry[2].mnemonic + entry[2].op_str);
    }

    return content;
}

void RelaPLTObject::init(char *buffer, uint64_t length) {
    uint64_t numOfEntry = length / sizeof(Elf64_Rela);
    this->entries.clear();

    for(uint64_t i = 0; i < numOfEntry; i++) {
        Elf64_Rela entry;
        std::memcpy(&entry, buffer + i * sizeof(Elf64_Rela), sizeof(Elf64_Rela));
        entries.push_back(entry);
    }
}

nlohmann::json RelaPLTObject::toJson() const {
    nlohmann::json content = nlohmann::json::array();
    for(const auto &relaPTLEntry : this->entries) {
        nlohmann::json entry;
        std::stringstream ss;
        ss << std::setw(16) << std::setfill('0') << std::hex << relaPTLEntry.r_offset;
        entry["Offset"]   = ss.str();
        entry["Type"]     = ELF64_R_TYPE(relaPTLEntry.r_info);
        entry["SymIndex"] = ELF64_R_SYM(relaPTLEntry.r_info);
        entry["Addend"]   = relaPTLEntry.r_addend;
        content.push_back(entry);
    }

    return content;
}

void DynsymObject::init(char *buffer, uint64_t length) {
    uint64_t numOfEntry = length / sizeof(Elf64_Sym);
    this->entries.clear();

    for(uint64_t i = 0; i < numOfEntry; i++) {
        Elf64_Sym entry;
        std::memcpy(&entry, buffer + i * sizeof(Elf64_Sym), sizeof(Elf64_Sym));
        entries.push_back(entry);
    }
}

nlohmann::json DynsymObject::toJson(const StrTable &dynStrTable) const {
    nlohmann::json content = nlohmann::json::array();
    for(const auto &dynsymEntry : this->entries) {
        nlohmann::json entry;
        entry["Value"] = dynsymEntry.st_value;
        entry["Size"]  = dynsymEntry.st_size;
        entry["Type"]  = ELF64_ST_TYPE(dynsymEntry.st_info);
        entry["Bind"]  = ELF64_ST_BIND(dynsymEntry.st_info);
        entry["Vis"]   = dynsymEntry.st_other;
        entry["Ndx"]   = dynsymEntry.st_shndx;
        entry["Name"]  = dynStrTable.get(dynsymEntry.st_name);
        content.push_back(entry);
    }

    return content;
}

void DynLibLoader::initDynstrTable(char *buffer, uint64_t length) {
    this->dynStrTable.init(buffer, length);
}

void DynLibLoader::initPLTObject(char *buffer, uint64_t addr, uint64_t length) {
    this->pltObject.init(buffer, addr, length);
}

void DynLibLoader::initPLTSECObject(char *buffer, uint64_t addr, uint64_t length) {
    this->pltSecObject.init(buffer, addr, length);
}

void DynLibLoader::initRelaPLTObject(char *buffer, uint64_t length) {
    this->relaPLTObject.init(buffer, length);
}

void DynLibLoader::initDynsymObject(char *buffer, uint64_t length) {
    this->dynsymObject.init(buffer, length);
}

void DynLibLoader::initLibFuncAddrInfo() {
    FIG_ASSERT(this->pltSecObject.entries.size() == this->relaPLTObject.entries.size(),
               "pltSecObject.entries.size()=", pltSecObject.entries.size(),
               "; relaPLTObject.entries.size()=", relaPLTObject.entries.size());
    for(uint32_t i = 0; i < this->pltSecObject.entries.size(); i++) {
        // .plt.sec entry one-to-one mapping .rela.plt entry
        uint64_t PLTSECAddr  = this->pltSecObject.entries[i][0].address;
        uint64_t dynsymIndex = ELF64_R_SYM(this->relaPLTObject.entries[i].r_info);
        uint64_t nameOffset  = this->dynsymObject.entries[dynsymIndex].st_name;
        std::string name     = this->dynStrTable.get(nameOffset);
        this->dynLibFuncAddrMap.insert({name, PLTSECAddr});
    }
}

nlohmann::json DynLibLoader::toJson() {
    nlohmann::json content = nlohmann::json::array();
    for(const auto &[name, addr] : this->dynLibFuncAddrMap) {
        nlohmann::json entry;
        entry["Addr"] = dec_to_hex(addr);
        entry["Name"] = name;
        content.push_back(entry);
    }

    return content;
}