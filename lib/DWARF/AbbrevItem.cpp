#include "DWARF/AbbrevItem.h"
#include "DWARF/AbbrevEntry.h" // for abbrev_entry
#include <iostream>            // for char_traits, basic_ostream, operator<<
#include "Utils/Buffer.h"      // for get_uint_length_leb128, write_leb128u
#include "Utils/ToStr.h"       // for dw_tag_to_str
#include "libdwarf/dwarf.h"

AbbrevItem::AbbrevItem() :
    offset(0), number(0), tag(0), has_children(false), entries(std::vector<AbbrevEntry>()) {};

AbbrevItem::AbbrevItem(const char *const buffer, int64_t &offset) {
    this->initWithBuffer(buffer, offset);
}

AbbrevItem::AbbrevItem(uint64_t number, uint64_t tag, bool has_children) {
    this->initWithBasicSetting(number, tag, has_children);
}

void AbbrevItem::initWithBuffer(const char *const buffer, int64_t &offset) {
    this->offset = offset;
    this->number = read_leb128u(buffer, offset);
    if(this->number == 0)
        return;
    this->tag          = read_leb128u(buffer, offset);
    this->has_children = !(read_u8(buffer, offset) == 0);
    while(true) {
        this->entries.emplace_back(buffer, offset);
        if(this->entries.back().isNull())
            break;
    }
}

bool AbbrevItem::isNull() {
    return this->number == 0;
}

void AbbrevItem::initWithBasicSetting(uint64_t number, uint64_t tag, bool has_children) {
    this->number       = number;
    this->tag          = tag;
    this->has_children = has_children;
}

int64_t AbbrevItem::getSize() {
    if(this->isNull()) {
        return get_uint_length_leb128(0);
    } else {
        int64_t res = get_uint_length_leb128(this->number) + get_uint_length_leb128(this->tag) + 1;
        for(auto &entry : this->entries) {
            res += entry.getSize();
        }
        return res;
    }
}

void AbbrevItem::write(char *buffer, int64_t &offset) {
    if(this->isNull()) {
        write_leb128u(0, buffer, offset);
    } else {
        write_leb128u(this->number, buffer, offset);
        write_leb128u(this->tag, buffer, offset);
        write_u8(this->has_children, buffer, offset);
        for(auto &entry : this->entries) {
            entry.write(buffer, offset);
        }
    }
}

void AbbrevItem::print() {
    std::cout << this->number << "\t";
    std::cout << dw_tag_to_str(this->tag) << "\t";
    std::cout << (this->has_children ? "[has children]" : "[no children]") << std::endl;
    for(auto &entry : this->entries) {
        entry.print();
    }
}

namespace DWARF {

    bool isVariableTag(uint64_t tag) {
        return tag == DW_TAG_variable;
    }

    bool isVariableAbbrev(const AbbrevItem &item) {
        if(!isVariableTag(item.tag))
            return false;

        bool has_name    = false;
        bool has_loction = false;
        bool has_type    = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_name)
                has_name = true;
            else if(entry.at == DW_AT_location)
                has_loction = true;
            else if(entry.at == DW_AT_type)
                has_type = true;
            else if(entry.at == DW_AT_abstract_origin || entry.at == DW_AT_specification) {
                has_name = true;
                has_type = true;
            }
        }

        return has_name && has_loction && has_type;
    }

    bool isParameterTag(uint64_t tag) {
        return tag == DW_TAG_formal_parameter;
    }

    bool isParameterAbbrev(const AbbrevItem &item) {
        if(!isParameterTag(item.tag))
            return false;

        bool has_name    = false;
        bool has_loction = false;
        bool has_type    = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_name)
                has_name = true;
            else if(entry.at == DW_AT_location)
                has_loction = true;
            else if(entry.at == DW_AT_type)
                has_type = true;
            else if(entry.at == DW_AT_abstract_origin || entry.at == DW_AT_specification) {
                has_name = true;
                has_type = true;
            }
        }

        return has_name && has_loction && has_type;
    }

    bool isAbsVarAbbrev(const AbbrevItem &item) {
        if(item.tag != DW_TAG_variable && item.tag != DW_TAG_formal_parameter)
            return false;

        [[maybe_unused]]
        bool has_name    = false;
        bool has_loction = false;
        bool has_type    = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_name)
                has_name = true;
            else if(entry.at == DW_AT_location)
                has_loction = true;
            else if(entry.at == DW_AT_type)
                has_type = true;
        }
        // abstract variable must contain type (has_type)
        // abstract variable may not have name (optionl has_name)
        // abstract variable must not contain location (!has_loction)
        return has_type && !has_loction;
    }

    bool isFuncTag(uint64_t tag) {
        return tag == DW_TAG_subprogram;
    }

    bool isFuncAbbrev(const AbbrevItem &item) {
        if(!isFuncTag(item.tag))
            return false;

        bool has_low_pc    = false;
        bool has_high_pc   = false;
        bool has_func_name = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_low_pc)
                has_low_pc = true;
            else if(entry.at == DW_AT_high_pc)
                has_high_pc = true;
            else if(entry.at == DW_AT_name || entry.at == DW_AT_linkage_name || entry.at == DW_AT_specification)
                has_func_name = true;
        }

        return has_low_pc && has_high_pc && has_func_name;
    }

    bool isAbsFuncAbbrev(const AbbrevItem &item) {
        if(item.tag != DW_TAG_subprogram)
            return false;

        bool has_low_pc    = false;
        bool has_high_pc   = false;
        bool has_func_name = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_low_pc)
                has_low_pc = true;
            else if(entry.at == DW_AT_high_pc)
                has_high_pc = true;
            else if(entry.at == DW_AT_name || entry.at == DW_AT_linkage_name)
                has_func_name = true;
        }

        return !has_low_pc && !has_high_pc && has_func_name;
    }

    bool isStructCommonMemberAbbrev(const AbbrevItem &item) {
        if(item.tag != DW_TAG_member)
            return false;

        bool has_type                 = false;
        bool has_data_member_location = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_type)
                has_type = true;
            else if(entry.at == DW_AT_data_member_location)
                has_data_member_location = true;
        }

        return has_data_member_location && has_type;
    }

    bool isStructBitMemberAbbrev(const AbbrevItem &item) {
        if(item.tag != DW_TAG_member)
            return false;

        bool has_type            = false;
        bool has_data_bit_offset = false;
        bool has_bit_size        = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_type)
                has_type = true;
            else if(entry.at == DW_AT_bit_size)
                has_bit_size = true;
            else if(entry.at == DW_AT_data_bit_offset)
                has_data_bit_offset = true;
        }

        return has_bit_size && has_data_bit_offset && has_type;
    }

    bool isUnionMemberAbbrev(const AbbrevItem &item) {
        if(item.tag != DW_TAG_member)
            return false;

        bool has_type = false;
        for(auto &entry : item.entries) {
            if(entry.at == DW_AT_type)
                has_type = true;
        }

        return has_type;
    }

    bool isTypeTag(uint64_t tag) {
        switch(tag) {
        case DW_TAG_unspecified_type: return true;
        case DW_TAG_base_type: return true;
        case DW_TAG_pointer_type: return true;
        case DW_TAG_reference_type: return true;
        case DW_TAG_ptr_to_member_type: return true;
        case DW_TAG_rvalue_reference_type: return true;
        case DW_TAG_const_type: return true;
        case DW_TAG_typedef: return true;
        case DW_TAG_restrict_type: return true;
        case DW_TAG_volatile_type: return true;
        case DW_TAG_array_type: return true;
        case DW_TAG_structure_type: return true;
        case DW_TAG_class_type: return true;
        case DW_TAG_enumeration_type: return true;
        case DW_TAG_union_type: return true;
        case DW_TAG_subroutine_type: return true;
        default: return false;
        }
    }

    bool isSingleType(const AbbrevItem &item) {
        switch(item.tag) {
        case DW_TAG_unspecified_type: return true;
        case DW_TAG_base_type: return true;
        case DW_TAG_pointer_type: return true;
        case DW_TAG_reference_type: return true;
        case DW_TAG_ptr_to_member_type: return true;
        case DW_TAG_rvalue_reference_type: return true;
        case DW_TAG_const_type: return true;
        case DW_TAG_typedef: return true;
        case DW_TAG_restrict_type: return true;
        case DW_TAG_volatile_type: return true;
        default: return false;
        }
    }

    bool isMultiType(const AbbrevItem &item) {
        switch(item.tag) {
        case DW_TAG_array_type: return true;
        case DW_TAG_structure_type: return true;
        case DW_TAG_class_type: return true;
        case DW_TAG_enumeration_type: return true;
        case DW_TAG_union_type: return true;
        case DW_TAG_subroutine_type: return true;
        default: return false;
        }
    }
} // namespace DWARF
