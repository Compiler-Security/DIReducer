#ifndef ABBREV_PARSER_H
#define ABBREV_PARSER_H

#include <stdint.h>     // for int64_t, uint32_t, uint8_t, uint64_t
struct AbbrevEntry;     // lines 5-5
struct CompilationUnit; // lines 6-6

struct CompilationUnit;

class AbbrevParser {
public:
    uint8_t addressSize;
    uint8_t bitWidth;
    uint64_t cuOffset;

private:
    uint32_t __get_length_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const;

public:
    uint32_t get_length_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const;
    void skip_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const;
    void print_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer, int64_t &offset) const;
    void print_abbrev_entry(const AbbrevEntry &abbrevEntry, const char *const buffer) const;

public:
    AbbrevParser()                     = default;
    AbbrevParser(const AbbrevParser &) = default;
    AbbrevParser(AbbrevParser &&)      = default;
    AbbrevParser(CompilationUnit *cu);

    ~AbbrevParser() = default;
};

#endif