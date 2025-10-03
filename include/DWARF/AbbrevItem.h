#ifndef ABBREV_ITEM_H
#define ABBREV_ITEM_H

#include <cstdint>
#include <stdint.h>      // for uint64_t, int64_t
#include <vector>        // for vector
#include "AbbrevEntry.h" // for AbbrevEntry

struct AbbrevItem {
    int64_t offset;
    uint64_t number;
    uint64_t tag;
    bool has_children;
    std::vector<AbbrevEntry> entries;

    AbbrevItem();
    AbbrevItem(const char *const buffer, int64_t &offset);
    AbbrevItem(uint64_t number, uint64_t tag, bool has_children);

    void initWithBuffer(const char *const buffer, int64_t &offset);

    void initWithBasicSetting(uint64_t number, uint64_t tag, bool has_children);

    bool isNull();

    int64_t getSize();

    void write(char *buffer, int64_t &offset);

    void print();
};

namespace DWARF {
    bool isVariableTag(uint64_t tag);

    bool isVariableAbbrev(const AbbrevItem &item);

    bool isParameterTag(uint64_t tag);

    bool isParameterAbbrev(const AbbrevItem &item);

    bool isAbsVarAbbrev(const AbbrevItem &item);

    bool isFuncTag(uint64_t tag);

    bool isFuncAbbrev(const AbbrevItem &item);

    bool isAbsFuncAbbrev(const AbbrevItem &item);

    bool isStructCommonMemberAbbrev(const AbbrevItem &item);

    bool isStructBitMemberAbbrev(const AbbrevItem &item);

    bool isUnionMemberAbbrev(const AbbrevItem &item);

    bool isTypeTag(uint64_t tag);

    bool isSingleType(const AbbrevItem &item);

    bool isMultiType(const AbbrevItem &item);
} // namespace DWARF

#endif