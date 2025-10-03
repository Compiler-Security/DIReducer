#include <stdint.h>       // for int64_t, uint32_t, uint64_t, uint8_t
#include <string.h>       // for strlen
#include <string>         // for string
#include "Utils/Buffer.h" // for LEB_128_MAX_SHIFT, write_leb128s, write...
#include "Fig/panic.hpp"
#include "Fig/debug.hpp"

uint8_t read_u8(const char *const buffer, int64_t &offset) {
    return *(buffer + offset++);
}

uint16_t read_u16(const char *const buffer, int64_t &offset) {
    uint16_t res = 0;
    res |= static_cast<uint16_t>(read_u8(buffer, offset));
    res |= static_cast<uint16_t>(read_u8(buffer, offset)) << 8;
    return res;
}

uint32_t read_u24(const char *const buffer, int64_t &offset) {
    uint32_t res = 0;
    res |= static_cast<uint32_t>(read_u8(buffer, offset));
    res |= static_cast<uint32_t>(read_u8(buffer, offset)) << 8;
    res |= static_cast<uint32_t>(read_u8(buffer, offset)) << 16;
    return res;
}

uint32_t read_u32(const char *const buffer, int64_t &offset) {
    uint32_t res = 0;
    res |= static_cast<uint32_t>(read_u16(buffer, offset));
    res |= static_cast<uint32_t>(read_u16(buffer, offset)) << 16;
    return res;
}

uint64_t read_u64(const char *const buffer, int64_t &offset) {
    uint64_t res = 0;
    res |= static_cast<uint64_t>(read_u32(buffer, offset));
    res |= static_cast<uint64_t>(read_u32(buffer, offset)) << 32;
    return res;
}

int8_t read_i8(const char *const buffer, int64_t &offset) {
    return static_cast<int8_t>(read_u8(buffer, offset));
}

int16_t read_i16(const char *const buffer, int64_t &offset) {
    return static_cast<int16_t>(read_u16(buffer, offset));
}

int32_t read_i32(const char *const buffer, int64_t &offset) {
    return static_cast<int32_t>(read_u32(buffer, offset));
}

int64_t read_i64(const char *const buffer, int64_t &offset) {
    return static_cast<int64_t>(read_u64(buffer, offset));
}

void write_u8(uint8_t value, char *buffer, int64_t &offset) {
    buffer[offset++] = value;
}

void write_u16(uint16_t value, char *buffer, int64_t &offset) {
    write_u8(value & 0xFF, buffer, offset);
    write_u8((value >> 8) & 0xFF, buffer, offset);
}
void write_u24(uint32_t value, char *buffer, int64_t &offset) {
    write_u8(value & 0xFF, buffer, offset);
    write_u8((value >> 8) & 0xFF, buffer, offset);
    write_u8((value >> 16) & 0xFF, buffer, offset);
}

void write_u32(uint32_t value, char *buffer, int64_t &offset) {
    write_u16(value & 0xFFFF, buffer, offset);
    write_u16((value >> 16) & 0xFFFF, buffer, offset);
}
void write_u64(uint64_t value, char *buffer, int64_t &offset) {
    write_u32(value & 0xFFFFFFFF, buffer, offset);
    write_u32((value >> 32) & 0xFFFFFFFF, buffer, offset);
}

void write_i8(int8_t value, char *buffer, int64_t &offset) {
    write_u8(value, buffer, offset);
}

void write_i16(int16_t value, char *buffer, int64_t &offset) {
    write_u16(value, buffer, offset);
}

void write_i24(int32_t value, char *buffer, int64_t &offset) {
    write_u24(value, buffer, offset);
}

void write_i32(int32_t value, char *buffer, int64_t &offset) {
    write_u32(value, buffer, offset);
}

void write_i64(int64_t value, char *buffer, int64_t &offset) {
    write_u64(value, buffer, offset);
}

std::string read_string(const char *const buffer, int64_t &offset) {
    const char *str = buffer + offset;
    offset += strlen(str) + 1;
    return str;
}

uint64_t read_leb128u(const char *const buffer, int64_t &offset) {
    uint64_t result = 0;
    uint8_t shift   = 0;
    while(true) {
        uint8_t byte = read_u8(buffer, offset);
        result |= ((uint64_t)byte & 0x7F) << shift;
        shift += 7;
        if((byte & 0x80) == 0) {
            break;
        }
    }

    FIG_DEBUG(
        if(shift > LEB_128_MAX_SHIFT) {
            FIG_PANIC("[error in read_leb128u]", "shift is larger than 192, ", "shift: ", shift);
        });

    return result;
}

int64_t read_leb128s(const char *const buffer, int64_t &offset) {
    int64_t result = 0;
    uint8_t shift  = 0;
    while(true) {
        uint8_t byte = read_u8(buffer, offset);
        result |= (((uint64_t)byte & 0x7F) << shift);
        shift += 7;
        if((byte & 0x80) == 0) {
            int s  = 64 - shift;
            result = (result << s) >> s;
            break;
        }
    }

    FIG_DEBUG(
        if(shift > LEB_128_MAX_SHIFT) {
            FIG_PANIC("[error in read_leb128s]", "shift is larger than 192, ", "shift: ", shift);
        });

    return result;
}

void write_leb128u(uint64_t value, char *buffer, int64_t &offset) {
    while(true) {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if(value == 0) {
            write_u8(byte, buffer, offset);
            break;
        }
        write_u8(byte | 0x80, buffer, offset);
    }
}

void write_leb128s(int64_t value, char *buffer, int64_t &offset) {
    while(true) {
        uint8_t byte  = value & 0x7F;
        bool signFlag = value & 0x40;
        value >>= 7;
        if((value == 0 && !signFlag) || (value == -1 && signFlag)) {
            write_u8(byte, buffer, offset);
            break;
        }
        write_u8(byte | 0x80, buffer, offset);
    }
}

void write_leb128u(uint64_t value, char *buffer) {
    int64_t offset = 0;
    write_leb128u(value, buffer, offset);
}

void write_leb128s(int64_t value, char *buffer) {
    int64_t offset = 0;
    write_leb128s(value, buffer, offset);
}

void write_leb128u_fixed_length(uint64_t value, char *buffer, int64_t &offset, uint32_t fixed_length) {
    if(fixed_length == 0)
        FIG_PANIC("in write_leb128u_fixed_length,", "parameter fixed_length is zero!");

    uint32_t length_written = 0;
    while(true) {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        length_written++;
        if(length_written < fixed_length) {
            write_u8(byte | 0x80, buffer, offset);
        } else {
            if(value == 0) {
                write_u8(byte, buffer, offset);
                break;
            } else {
                FIG_PANIC(
                    "in write_leb128u_fixed_length,", "parameter fixed_length is too small!", "\n"
                                                                                              "fixed_length: ",
                    fixed_length, "\n"
                                  "length_written: ",
                    length_written, "\n"
                                    "value: ",
                    value, "\n");
            }
        }
    }
}

void write_leb128s_fixed_length(int64_t value, char *buffer, int64_t &offset, uint32_t fixed_length) {
    if(fixed_length == 0)
        FIG_PANIC("in write_leb128s_fixed_length,", "parameter fixed_length is zero!");

    uint32_t length_written = 0;
    while(true) {
        uint8_t byte  = value & 0x7F;
        bool signFlag = value & 0x40;
        value >>= 7;
        length_written++;
        if(length_written < fixed_length) {
            write_u8(byte | 0x80, buffer, offset);
        } else {
            if((value == 0 && !signFlag) || (value == -1 && signFlag)) {
                write_u8(byte, buffer, offset);
                break;
            } else {
                FIG_PANIC(
                    "in write_leb128s_fixed_length,", "parameter fixed_length is too small!", "\n"
                                                                                              "fixed_length: ",
                    fixed_length, "\n"
                                  "length_written: ",
                    length_written, "\n"
                                    "value: ",
                    value, "\n");
            }
        }
    }
}

uint8_t __get_length_leb128(const char *const buffer, int64_t &offset) {
    int length                     = 0;
    [[maybe_unused]] uint8_t shift = 0;
    ;
    while(true) {
        length++;
        FIG_DEBUG(
            shift += 7;);
        if((read_u8(buffer, offset) & 0x80) == 0)
            break;
    }

    FIG_DEBUG(
        if(shift > LEB_128_MAX_SHIFT) {
            FIG_PANIC("[error in __get_length_leb128]", "shift is larger than 192, ", "shift: ", shift);
        });

    return length;
}

int get_uint_length_leb128(uint64_t num) {
    int res = 0;
    while(true) {
        num >>= 7;
        res++;
        if(num == 0)
            break;
    }

    return res;
}

int get_int_length_leb128(int64_t num) {
    int res = 0;
    while(true) {
        bool signFlag = num & 0x40;
        num >>= 7;
        res++;
        if((num == 0 && !signFlag) || (num == -1 && signFlag))
            break;
    }

    return res;
}

uint8_t get_length_leb128(const char *const buffer, int64_t &offset) {
    int64_t originOffset = offset;
    int length           = __get_length_leb128(buffer, offset);
    offset               = originOffset;
    return length;
}

void skip_leb128(const char *const buffer, int64_t &offset) {
    int length = get_length_leb128(buffer, offset);
    offset += length;
}
