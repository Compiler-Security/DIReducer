#ifndef LIBCFG_UTILS_H
#define LIBCFG_UTILS_H

#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>
#include "capstone/x86.h"

namespace CFG {
    template<typename T>
    std::string dec_to_hex(T dec) {
        std::string res;
        std::stringstream ss;
        ss << std::hex << dec;
        ss >> res;
        return res;
    }

    template<typename T>
    std::string dec_to_hex(uint8_t dec) {
        std::string res;
        std::stringstream ss;
        ss << std::hex << static_cast<uint16_t>(dec);
        ss >> res;
        return res;
    }

    bool isHexadecimal(const std::string &str);

    uint64_t hexStringToUInt(const std::string &hexStr);
    
    const char *x86_op_type_to_string(x86_op_type type);

    const char *x86_reg_to_string(x86_reg reg);

    std::string x86_op_mem_to_string(x86_op_mem mem);

    std::string cs_x86_op_to_string(cs_x86_op op);

    std::string get_demangled_name(const std::string &name);

    void json_to_file(nlohmann::json &njson, std::string path);

    uint32_t getUniqueID();
} // namespace CFG

#endif