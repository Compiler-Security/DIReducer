#ifndef DWARF_OP_H
#define DWARF_OP_H

#include <stdint.h>
#include "Utils/Buffer.h"

class DWARFOP {
public:
    DWARFOP() = default;
    DWARFOP(const char *buffer, int64_t &offset, uint8_t bitWidth, uint8_t addressSize) {
        this->init(buffer, offset, bitWidth, addressSize);
    }
    ~DWARFOP() = default;

    uint8_t operation;
    union {
        uint64_t u;
        int64_t s;
    } operand1;
    union {
        uint64_t u;
        int64_t s;
    } operand2;
    uint16_t operandSize1;
    uint16_t operandSize2;
    common_buffer blockBuffer;

    void init(const char *buffer, int64_t &offset, uint8_t bitWidth, uint8_t addressSize);
    void print();
    void write(char *buffer, int64_t &offset);
};

#endif