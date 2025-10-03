#ifndef LOCLISTS_OBJECT_H
#define LOCLISTS_OBJECT_H

#include "DWARFBuffer.h"
#include "LoclistsList.h"


struct LoclistsObject {
    dr_immut<char *> loclistsBuffer;
    int64_t loclistsSize;
    std::vector<LoclistsList> loclistsObject;

    LoclistsObject() = default;
    LoclistsObject(DWARFBuffer &dwarfBuffer);
    ~LoclistsObject() = default;

    void init(DWARFBuffer &dwarfBuffer);

    uint64_t get_target_loclist_offset(uint64_t loclists_base, uint64_t relative_offset);

    void print();

    void write(char *buffer, int64_t &offset);
};

#endif