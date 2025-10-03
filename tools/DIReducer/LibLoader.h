#ifndef LIBLOADER_H
#define LIBLOADER_H

#include "DWARF/ELFObject.h"
#include "capstone/capstone.h"
#include "nlohmann/json_fwd.hpp"
#include <array>
#include <cstdint>
#include "libelf/elf.h"
#include <string>
#include <unordered_map>
#include <vector>

class PLTObject {
public:
    PLTObject() = default;
    void init(char *buffer, uint64_t addr, uint64_t length);

    nlohmann::json toJson() const;

public:
    std::array<cs_insn, 3> header;
    std::vector<std::array<cs_insn, 4>> entries;
};

class PLTSECObject {
public:
    PLTSECObject() = default;
    void init(char *buffer, uint64_t addr, uint64_t length);

    nlohmann::json toJson() const;

public:
    std::vector<std::array<cs_insn, 3>> entries;
};

class RelaPLTObject {
public:
    RelaPLTObject() = default;
    void init(char *buffer, uint64_t length);
    
    nlohmann::json toJson() const;

public:
    std::vector<Elf64_Rela> entries;
};

class DynsymObject {
public:
    DynsymObject() = default;
    void init(char *buffer, uint64_t length);

    nlohmann::json toJson(const StrTable& dynStrTable) const;

public:
    std::vector<Elf64_Sym> entries;
};

class DynLibLoader {
public:
    DynLibLoader() = default;
    void initDynstrTable(char *buffer, uint64_t length);
    void initPLTObject(char *buffer, uint64_t addr, uint64_t length);
    void initPLTSECObject(char *buffer, uint64_t addr, uint64_t length);
    void initRelaPLTObject(char *buffer, uint64_t length);
    void initDynsymObject(char *buffer, uint64_t length);
    void initLibFuncAddrInfo();
    nlohmann::json toJson();

private:
    StrTable dynStrTable;
    PLTObject pltObject;
    PLTSECObject pltSecObject;
    RelaPLTObject relaPLTObject;
    DynsymObject dynsymObject;

public:
    std::unordered_map<std::string, uint64_t> dynLibFuncAddrMap;
};

#endif