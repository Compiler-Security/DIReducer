#include <libdwarf/dwarf.h>    // for DW_FORM_implicit_const
#include <stdint.h>            // for int64_t
#include <iostream>            // for basic_ostream, operator<<, char_traits
#include "DWARF/AbbrevEntry.h" // for abbrev_entry
#include "Utils/Buffer.h"   // for get_uint_length_leb128, read_leb128u
#include "Utils/ToStr.h"      // for dw_at_to_str, dw_form_to_str

AbbrevEntry::AbbrevEntry() {
    this->initWithBasicSetting(0, 0, 0);
}

AbbrevEntry::AbbrevEntry(uint64_t at, uint64_t form, int64_t implicit_const) {
    this->initWithBasicSetting(at, form, implicit_const);
}

AbbrevEntry::AbbrevEntry(const char *const buffer, int64_t &offset) {
    this->at   = read_leb128u(buffer, offset);
    this->form = read_leb128u(buffer, offset);
    // special case
    if(this->form == DW_FORM_implicit_const) {
        this->implicit_const = read_leb128s(buffer, offset);
    }
}

void AbbrevEntry::initWithBasicSetting(uint64_t at, uint64_t form, int64_t implicit_const) {
    this->at             = at;
    this->form           = form;
    this->implicit_const = implicit_const;
}

int64_t AbbrevEntry::getSize() {
    if(this->form == DW_FORM_implicit_const)
        return get_uint_length_leb128(this->at) + get_uint_length_leb128(this->form) + get_int_length_leb128(this->implicit_const);
    else
        return get_uint_length_leb128(this->at) + get_uint_length_leb128(this->form);
}

void AbbrevEntry::write(char *buffer, int64_t &offset) {
    write_leb128u(this->at, buffer, offset);
    write_leb128u(this->form, buffer, offset);
    if(this->form == DW_FORM_implicit_const)
        write_leb128s(this->implicit_const, buffer, offset);
}

void AbbrevEntry::print() {
    if(this->isNull()) {
        std::cout << "DW_AT value: 0"
                  << "\t\t";
        std::cout << "DW_FORM value: 0" << std::endl;
    } else {
        std::cout << dw_at_to_str(this->at) << "\t\t";
        std::cout << dw_form_to_str(this->form);
        if(this->form == DW_FORM_implicit_const) {
            std::cout << ": " << this->implicit_const;
        }
        std::cout << std::endl;
    }
}

bool AbbrevEntry::isNull() const {
    return (at == 0 && form == 0);
}