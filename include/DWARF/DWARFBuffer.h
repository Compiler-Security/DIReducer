#ifndef DWARF_BUFFER_H
#define DWARF_BUFFER_H

#include <stdint.h>           // for int64_t
#include <memory>             // for shared_ptr
#include "Utils/Struct.h"  // for dr_immut
class ELFObjetc;

class DWARFBuffer {
private:
    std::shared_ptr<char[]> elfBuffer;

public:
    dr_immut<char *> infoBuffer;
    dr_immut<int64_t> infoSize;
    dr_immut<char *> abbrevBuffer;
    dr_immut<int64_t> abbrevSize;
    dr_immut<char *> lineBuffer;
    dr_immut<int64_t> lineSize;
    dr_immut<char *> loclistsBuffer;
    dr_immut<int64_t> loclistsSize;
    dr_immut<char *> lineStrBuffer;
    dr_immut<int64_t> lineStrSize;

public:
    void print();

public:
    DWARFBuffer();

    DWARFBuffer(ELFObjetc &elfObject);

    ~DWARFBuffer() = default;

    void init(ELFObjetc &elfObject);
};

#endif