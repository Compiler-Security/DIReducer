#ifndef INFO_ENTRY_H
#define INFO_ENTRY_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdint.h>

#include "AbbrevEntry.h"
#include "AbbrevList.h"
#include "AbbrevItem.h"
#include "AbbrevParser.h"
#include "LoclistsObject.h"
#include "AttrBuffer.h"
#include "ELFObject.h"

struct InfoEntry {
    uint16_t depth;
    uint64_t cuOffset;
    uint64_t innerOffset;
    uint16_t module;

    AbbrevItem abbrevItem;
    std::vector<InfoEntry> children;

    uint64_t abbrevNumber;
    std::vector<AttrBuffer> attrBuffers;

    std::shared_ptr<AbbrevParser> abbrevParser;

public:
    InfoEntry()                  = default;
    InfoEntry(const InfoEntry &) = delete;
    InfoEntry(InfoEntry &&)      = default;
    InfoEntry(
        const char *const buffer,
        int64_t &offset,
        std::shared_ptr<AbbrevParser> abbrevParser,
        const std::shared_ptr<AbbrevList> &abbrevList,
        uint16_t depth,
        uint16_t module);

    ~InfoEntry() = default;

    void init(
        const char *const buffer,
        int64_t &offset,
        std::shared_ptr<AbbrevParser> abbrevParser,
        const std::shared_ptr<AbbrevList> &abbrevList,
        uint16_t depth,
        uint16_t module);

    void print() const;
    void print_tag() const;
    void print_attr() const;
    bool isNull() const;
    int64_t getSize() const;

    int64_t get_all_attr_count() const;

    int64_t get_attr_count() const;

    uint64_t get_global_offset() const;

    uint64_t get_local_offset() const;

    void write(char *buffer, int64_t &offset);
    void fix_offset(
        const int64_t &cuOffset,
        int64_t &curOffset,
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
        std::shared_ptr<LoclistsObject> &loclistsObject,
        uint64_t loclists_base);

private:
    int64_t get_abbrev_num_size() const;
    int64_t get_raw_data_size() const;
    int64_t get_children_size() const;

    void __fix_reference_ref1(
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator);

    void __fix_reference_ref2(
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator);

    void __fix_reference_ref4(
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator);

    void __fix_reference_ref8(
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator);

    void __fix_reference_ref_udata(
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator);

    void __fix_reference_ref_addr(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator);

    void __fix_reference_exprloc(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator);

    void __fix_reference_loclists(
        const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
        const std::unordered_map<int64_t, int64_t> &DIEOffsetMap,
        const std::vector<AttrBuffer>::iterator &bufferIterator,
        const std::shared_ptr<LoclistsObject> &loclistsObject,
        std::unordered_set<uint64_t> &loclistOffsetFixedSet,
        uint64_t loclists_base,
        uint64_t form);
};

std::string get_dw_at_name(const AbbrevEntry &entry, const AttrBuffer &buffer, const StrTable &table);
uint32_t get_dw_at_byte_size(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint64_t get_dw_at_type(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint64_t get_dw_at_abs_ori(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint64_t get_dw_at_upper_bound(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint64_t get_dw_at_low_pc(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint64_t get_dw_at_high_pc(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint64_t get_dw_at_encoding(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint32_t get_dw_at_data_member_location(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint32_t get_dw_at_data_bit_offset(const AbbrevEntry &entry, const AttrBuffer &buffer);
uint32_t get_dw_at_bit_size(const AbbrevEntry &entry, const AttrBuffer &buffer);

#endif