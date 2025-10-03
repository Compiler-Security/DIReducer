#ifndef DWARF_EXPR_PARSER_H
#define DWARF_EXPR_PARSER_H

#include <stdint.h>       // for int64_t, uint8_t
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector
#include "DWARFOP.h"     // for dwarf_op

class DWARFExprParser {
public:
    std::vector<DWARFOP> op_list;
    uint8_t bitWidth;
    uint8_t addressSize;

public:
    DWARFExprParser() = default;
    DWARFExprParser(const char *buffer, int64_t exprSize, uint8_t bitWidth, uint8_t addressSize);

    ~DWARFExprParser() = default;

    void init(const char *buffer, int64_t exprSize, uint8_t bitWidth, uint8_t addressSize);

    void print();

    void clear();

    void fix_reference_to_die(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap);

    void write(char *buffer, int64_t &offset);
};

#endif