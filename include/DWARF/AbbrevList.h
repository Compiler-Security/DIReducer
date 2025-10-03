#ifndef ABBREV_LIST_H
#define ABBREV_LIST_H

#include <stdint.h>       // for int64_t, uint64_t
#include <vector>         // for vector
#include "AbbrevItem.h"  // for abbrev_item

struct AbbrevList {
    uint64_t offset;
    std::vector<AbbrevItem> abbrevItemList;

    AbbrevList() = default;
    AbbrevList(const char *const buffer, int64_t &offset);

    AbbrevItem &get_abbrev_item_by_number(uint64_t abbrevNumber);

    int64_t getSize();

    void write(char *buffer, int64_t &offset);

    void print();
};

#endif