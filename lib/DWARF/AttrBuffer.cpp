#include "DWARF/AttrBuffer.h"
#include <cstring> // for memcpy

AttrBuffer::AttrBuffer(char *buffer, int64_t offset, uint32_t size) :
    buffer(buffer), offset(offset), size(size) {
}

AttrBuffer::AttrBuffer(const AttrBuffer &newAttrBuffer) {
    this->buffer = std::make_unique<char[]>(newAttrBuffer.size);
    std::memcpy(this->buffer.get(), newAttrBuffer.buffer.get(), newAttrBuffer.size);
    this->offset = newAttrBuffer.offset;
    this->size   = newAttrBuffer.size;
}

AttrBuffer::AttrBuffer(AttrBuffer &&movedAttrBuffer) {
    this->buffer.swap(movedAttrBuffer.buffer);
    this->offset = movedAttrBuffer.offset;
    this->size = movedAttrBuffer.size;
}

AttrBuffer &AttrBuffer::operator=(const AttrBuffer &newAttrBuffer) {
    this->buffer = std::make_unique<char[]>(newAttrBuffer.size);
    std::memcpy(this->buffer.get(), newAttrBuffer.buffer.get(), newAttrBuffer.size);
    this->offset = newAttrBuffer.offset;
    this->size   = newAttrBuffer.size;
    return *this;
}