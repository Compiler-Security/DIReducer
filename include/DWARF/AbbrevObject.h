#ifndef ABBREV_OBJECT_H
#define ABBREV_OBJECT_H

#include <stdint.h>     // for int64_t, uint64_t
#include <map>          // for map
#include <memory>       // for shared_ptr
#include "AbbrevList.h" // for abbrev_list
class DWARFBuffer;


struct AbbrevObject {
    std::map<uint64_t, std::shared_ptr<AbbrevList>> abbrevlistMap;

    AbbrevObject() = default;
    AbbrevObject(DWARFBuffer &dwarfBuffer);

    ~AbbrevObject() = default;

    void init(DWARFBuffer &dwarfBuffer);

    std::shared_ptr<AbbrevList> getAbbrevList(uint64_t offset);

    int64_t getSize();

    void write(char *buffer, int64_t &offset);

    void fix_offset();

    void print();
};

#endif