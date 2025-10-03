#ifndef COMPILATION_UNIT_H
#define COMPILATION_UNIT_H

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>

#include "Utils/Struct.h"
#include "AbbrevObject.h"
#include "AbbrevList.h"
#include "InfoEntry.h"
#include "AbbrevParser.h"
#include "LoclistsObject.h"

/**
 * Reference:[DWARF5.pdf p217]
 */
struct CompilationUnit {
public:
    uint64_t offset;
    uint8_t bitWidth;
    uint16_t module;
    uint64_t loclists_base;

    /**
     * unit header
     */
    uint64_t length;
    uint16_t version;
    uint8_t unit_type;
    uint8_t addressSize;
    uint64_t abbrevOffset;
    union {
        uint64_t dwoId;
        uint64_t typeSignature;
    };
    uint64_t typeOffset;

    /**
     * ref to abbrevList
     */
    std::shared_ptr<AbbrevList> abbrevList;

    /**
     * Info entry
     */
    InfoEntry infoEntry;

    dr_immut<const char *> infoBuffer;

    std::shared_ptr<AbbrevParser> abbrevParser;

private:
    void __init_unit_header_dwarf_4(const char *const buffer, int64_t &offset);
    void __init_unit_header_dwarf_5(const char *const buffer, int64_t &offset);

public:
    void init_unit_header(const char *const buffer, int64_t &offset);
    void print();
    void print_offset();
    void print_unit_header();
    int get_unit_header_size() const;
    int getLengthSize();
    int64_t getSize();
    void fix_offset(
        int64_t &cuOffset,
        std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        std::unordered_map<int64_t, int64_t> &DIEOffsetMap);
    void fix_reference_die_attr(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::unordered_map<uint64_t, uint64_t> &CUOffsetMap);

    void fix_reference_die_loclists(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        std::unordered_set<uint64_t> &loclist_offset_fixed_set,
        std::shared_ptr<LoclistsObject> &loclistsObject);

public:
    CompilationUnit()                         = default;
    CompilationUnit(const CompilationUnit &) = delete;
    CompilationUnit(CompilationUnit &&)      = default;
    CompilationUnit(const char *const buffer, int64_t &offset, const std::shared_ptr<AbbrevObject> &abbrevObjectRef, uint16_t id);

    ~CompilationUnit() = default;

    void write(char *buffer, int64_t &offset);

private:
    void write_unit_header(char *buffer, int64_t &offset);
};

#endif