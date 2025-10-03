#ifndef LOCLISTS_ENTRY_H
#define LOCLISTS_ENTRY_H

#include <stdint.h>  // for uint64_t, uint8_t, int64_t

struct LoclistsEntry {
    /* addition */
    int64_t offset;

    /* data */
    uint8_t type;
    union {
        uint64_t operand1;
        uint64_t address1;
    };
    union {
        uint64_t operand2;
        uint64_t address2;
    };
    uint64_t blockSize;
    char *blockBuffer;

private:
    void reset();

public:
    LoclistsEntry() = default;
    LoclistsEntry(const char *buffer, int64_t &offset, uint8_t addressSize, uint8_t bitWidth);

    void print();

    void init(const char *buffer, int64_t &offset, uint8_t addressSize, [[maybe_unused]] uint8_t bitWidth);
};

#endif