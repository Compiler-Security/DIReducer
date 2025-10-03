#ifndef ATTR_BUFFER_H
#define ATTR_BUFFER_H

#include <stdint.h> // for int64_t, uint32_t
#include <memory>   // for unique_ptr

struct AttrBuffer {
    std::unique_ptr<char[]> buffer;
    int64_t offset;
    uint32_t size;

    AttrBuffer() = default;
    AttrBuffer(char *buffer, int64_t offset, uint32_t size);
    AttrBuffer(const AttrBuffer &newAttrBuffer);
    AttrBuffer(AttrBuffer &&movedAttrBuffer);
    ~AttrBuffer() = default;

    AttrBuffer &operator=(const AttrBuffer &newAttrBuffer);
};

#endif