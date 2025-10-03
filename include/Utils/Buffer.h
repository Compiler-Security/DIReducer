#ifndef UTILS_BUFFER_H
#define UTILS_BUFFER_H

#include <stdint.h>
#include <string>
#include <memory>
#include <cstring>

#define LEB_128_MAX_SHIFT 192

uint8_t read_u8(const char *const buffer, int64_t &offset);
uint16_t read_u16(const char *const buffer, int64_t &offset);
uint32_t read_u24(const char *const buffer, int64_t &offset);
uint32_t read_u32(const char *const buffer, int64_t &offset);
uint64_t read_u64(const char *const buffer, int64_t &offset);
int8_t read_i8(const char *const buffer, int64_t &offset);
int16_t read_i16(const char *const buffer, int64_t &offset);
int32_t read_i32(const char *const buffer, int64_t &offset);
int64_t read_i64(const char *const buffer, int64_t &offset);
std::string read_string(const char *const buffer, int64_t &offset);

uint64_t read_leb128u(const char *const buffer, int64_t &offset);
int64_t read_leb128s(const char *const buffer, int64_t &offset);

void write_leb128u(uint64_t value, char *buffer, int64_t &offset);
void write_leb128s(int64_t value, char *buffer, int64_t &offset);
void write_leb128u(uint64_t value, char *buffer);
void write_leb128s(int64_t value, char *buffer);
void write_leb128u_fixed_length(uint64_t value, char *buffer, int64_t &offset, uint32_t fixed_length);
void write_leb128s_fixed_length(int64_t value, char *buffer, int64_t &offset, uint32_t fixed_length);
void write_u8(uint8_t value, char *buffer, int64_t &offset);
void write_u16(uint16_t value, char *buffer, int64_t &offset);
void write_u24(uint32_t value, char *buffer, int64_t &offset);
void write_u32(uint32_t value, char *buffer, int64_t &offset);
void write_u64(uint64_t value, char *buffer, int64_t &offset);
void write_i8(int8_t value, char *buffer, int64_t &offset);
void write_i16(int16_t value, char *buffer, int64_t &offset);
void write_i24(int32_t value, char *buffer, int64_t &offset);
void write_i32(int32_t value, char *buffer, int64_t &offset);
void write_i64(int64_t value, char *buffer, int64_t &offset);

uint8_t __get_length_leb128(const char *const buffer, int64_t &offset);
uint8_t get_length_leb128(const char *const buffer, int64_t &offset);
void skip_leb128(const char *const buffer, int64_t &offset);
int get_uint_length_leb128(uint64_t num);
int get_int_length_leb128(int64_t num);


struct common_buffer {
    std::shared_ptr<char[]> buffer;
    int64_t size;

    common_buffer() = default;
    common_buffer(char *buffer, int64_t size) {
        this->init(buffer, size);
    }
    common_buffer(const common_buffer &) = default;
    common_buffer(common_buffer &&)      = default;

    void init(char *buffer, int64_t size) {
        this->buffer.reset(buffer);
        this->size = size;
    }

    ~common_buffer() = default;
};

#endif