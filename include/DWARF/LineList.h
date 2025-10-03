#ifndef DR_LINE_LIST_H
#define DR_LINE_LIST_H

#include <cstdint>                // for uint64_t, uint8_t, int64_t, uint16_t
#include <string>                 // for string
#include <utility>                // for pair
#include <vector>                 // for vector
#include "LineStateMachine.h"   // for line_state_machine
#include "nlohmann/json_fwd.hpp"  // for json
struct LineTable;
struct StrTable;

struct LineList {
public:
    /* More */

    int64_t ll_offset;
    uint8_t bitWidth;
    uint8_t module;

    /* Line Number Program Header */

    uint64_t length;
    uint16_t version;
    uint8_t addressSize;
    uint8_t segmentSelectorSize;
    uint64_t header_length;
    uint8_t minimun_instruction_length;
    uint8_t maximum_operation_per_instruction;
    uint8_t default_is_stmt;
    int8_t line_base;
    uint8_t line_range;
    uint8_t opcode_base;
    std::vector<uint8_t> standard_opcode_lengths;
    uint8_t directory_entry_format_count;
    std::vector<std::pair<uint64_t, uint64_t>> directory_entry_format;
    uint64_t directories_count;
    uint8_t file_name_entry_format_count;
    std::vector<std::pair<uint64_t, uint64_t>> file_name_entry_format;
    uint64_t file_name_count;

    /* Additonal */

    std::vector<std::string> directories_str;
    std::vector<std::string> file_names_str;
    std::vector<std::string> file_names_directory_name_str;

    LineStateMachine state_machine;

public:
    LineList() = default;
    LineList(const char *const buffer, int64_t &offset, uint16_t id, StrTable &lineStrTable, LineTable &lineTable);

    nlohmann::json to_json() const;

private:
    void run(const char *const buffer, int64_t &offset, LineTable &lineTable);

    void init_state_machine();

    void init_header(const char *const buffer, int64_t &offset, uint16_t module, StrTable &lineStrTable);

    uint64_t get_dw_lnct_path(const char *const buffer, int64_t &offset, uint64_t form);

    uint64_t get_dw_lcnt_direatory_index(const char *const buffer, int64_t &offset, uint64_t form);
};

#endif