#ifndef LOCLISTS_LIST_H
#define LOCLISTS_LIST_H

#include <stdint.h>
#include <vector>

struct LoclistsList {
    /* addtion */
    int64_t offset;
    uint8_t bitWidth;
    uint64_t offsetArrayBase;
    /* data */
    int64_t length;
    uint16_t version;
    uint8_t addressSize;
    uint8_t segmentSelectorSize;
    uint32_t offsetEntryCount;
    std::vector<int64_t> offsetArray;

    LoclistsList() = default;
    LoclistsList(const char *buffer, int64_t &offset);
    ~LoclistsList() = default;

    void init(const char *buffer, int64_t &offset);
    void init_header(const char *buffer, int64_t &offset);
    void init_offset_array(const char *buffer, int64_t &offset);
    void print();
    void print_offset();
    void print_header();
    void print_offset_array();
};

#endif