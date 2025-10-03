#ifndef ABBREV_ENTRY_H
#define ABBREV_ENTRY_H

#include <cstdint>
#include <stdint.h>

struct AbbrevEntry {
    uint64_t at;
    uint64_t form;
    int64_t implicit_const;

    AbbrevEntry();
    AbbrevEntry(uint64_t at, uint64_t form, int64_t implicit_const);
    AbbrevEntry(const char *const buffer, int64_t &offset);

    void initWithBasicSetting(uint64_t at, uint64_t form, int64_t implicit_const);

    int64_t getSize();

    void write(char *buffer, int64_t &offset);

    bool isNull() const;

    void print();
};

#endif