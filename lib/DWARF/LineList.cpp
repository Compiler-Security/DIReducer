#include "DWARF/LineList.h"
#include "DWARF/ELFObject.h"        // for StrTable
#include "DWARF/LineStateMachine.h" // for LineStateMachine
#include "DWARF/LineTable.h"        // for LineTable
#include "Fig/panic.hpp"            // for FIG_PANIC
#include "Utils/Buffer.h"           // for read_u8, read_leb128u, read_u32
#include "libdwarf/dwarf.h"         // for DW_LNCT_path, DW_FORM_data1
#include "nlohmann/json_fwd.hpp"    // for json

LineList::LineList(const char *const buffer, int64_t &offset, uint16_t id, StrTable &lineStrTable, LineTable &lineTable) {
    this->init_header(buffer, offset, id, lineStrTable);
    this->init_state_machine();
    this->run(buffer, offset, lineTable);
};

nlohmann::json LineList::to_json() const {
    nlohmann::json content = {
        {"module", this->module},
        {"length", this->length},
        {"version", this->version},
        {"bitWidth", this->bitWidth},
        {"addressSize", this->addressSize},
        {"segmentSelectorSize", this->segmentSelectorSize},
        {"header_length", this->header_length},
        {"minimun_instruction_length", this->minimun_instruction_length},
        {"maximum_operation_per_instruction", this->maximum_operation_per_instruction},
        {"default_is_stmt", this->default_is_stmt},
        {"line_base", this->line_base},
        {"line_range", this->line_range},
        {"opcode_base", this->opcode_base},
        {"directory_entry_format_count", this->directory_entry_format_count},
        {"directories_count", this->directories_count},
        {"file_name_entry_format_count", this->file_name_entry_format_count},
        {"file_name_count", this->file_name_count},
        {"directories", nlohmann::json(this->directories_str)},
        {"file_names", nlohmann::json(this->file_names_str)},
    };

    return content;
}

void LineList::run(const char *const buffer, int64_t &offset, LineTable &lineTable) {
    int64_t end = this->ll_offset + this->length;
    if(this->bitWidth == 64)
        end += 12;
    else
        end += 4;

    bool flag_copy  = false;
    bool flag_clear = false;
    while(offset < end) {
        flag_copy = this->state_machine.operation(buffer, offset, flag_clear);
        if(flag_copy)
            lineTable.insert(this->state_machine.address, this->state_machine.line, this->state_machine.column, this->file_names_str[this->state_machine.file]);
        if(flag_clear)
            this->state_machine.reset_registers();
    }
}

void LineList::init_state_machine() {
    this->state_machine.reset_registers();
    this->state_machine.is_stmt                           = this->default_is_stmt;
    this->state_machine.addressSize                       = this->addressSize;
    this->state_machine.minimun_instruction_length        = this->minimun_instruction_length;
    this->state_machine.maximum_operation_per_instruction = this->maximum_operation_per_instruction;
    this->state_machine.default_is_stmt                   = this->default_is_stmt;
    this->state_machine.line_base                         = this->line_base;
    this->state_machine.line_range                        = this->line_range;
    this->state_machine.opcode_base                       = this->opcode_base;
}

void LineList::init_header(const char *const buffer, int64_t &offset, uint16_t module, StrTable &lineStrTable) {
    this->module    = module;
    this->ll_offset = offset;
    this->length    = read_u32(buffer, offset);
    // 判断64/32 bit的保留数字 0xffffffff
    if(this->length == 0xffffffff) {
        this->length   = read_u64(buffer, offset);
        this->bitWidth = 64;
    } else {
        this->bitWidth = 32;
    }
    this->version     = read_u16(buffer, offset);
    this->addressSize = read_u8(buffer, offset);
    if(this->addressSize != 8 && this->addressSize != 4)
        FIG_PANIC("Unsupported addressSize: ", static_cast<uint64_t>(this->addressSize));
    this->segmentSelectorSize = read_u8(buffer, offset);
    if(this->bitWidth == 32)
        this->header_length = read_u32(buffer, offset);
    else
        this->header_length = read_u64(buffer, offset);
    this->minimun_instruction_length        = read_u8(buffer, offset);
    this->maximum_operation_per_instruction = read_u8(buffer, offset);
    this->default_is_stmt                   = read_u8(buffer, offset);
    this->line_base                         = read_i8(buffer, offset);
    this->line_range                        = read_u8(buffer, offset);
    this->opcode_base                       = read_u8(buffer, offset);
    for(uint8_t i = 1; i < this->opcode_base; i++) {
        this->standard_opcode_lengths.push_back(read_u8(buffer, offset));
    }

    this->directory_entry_format_count = read_u8(buffer, offset);
    for(uint8_t i = 0; i < this->directory_entry_format_count; i++) {
        uint64_t content_type = read_leb128u(buffer, offset);
        uint64_t form         = read_leb128u(buffer, offset);
        this->directory_entry_format.push_back(std::make_pair(content_type, form));
    }

    this->directories_count = read_leb128u(buffer, offset);
    for(uint64_t i = 0; i < this->directories_count; i++) {
        for(auto &p : this->directory_entry_format) {
            switch(p.first) {
            case DW_LNCT_path: {
                uint64_t str_offset = this->get_dw_lnct_path(buffer, offset, p.second);
                this->directories_str.push_back(lineStrTable.get(str_offset));
                break;
            }
            default: FIG_PANIC("Unknown DW_LNCT for directory format");
            }
        }
    }

    this->file_name_entry_format_count = read_u8(buffer, offset);
    for(uint8_t i = 0; i < this->file_name_entry_format_count; i++) {
        uint64_t content_type = read_leb128u(buffer, offset);
        uint64_t form         = read_leb128u(buffer, offset);
        this->file_name_entry_format.push_back(std::make_pair(content_type, form));
    }

    this->file_name_count = read_leb128u(buffer, offset);
    for(uint64_t i = 0; i < this->file_name_count; i++) {
        for(auto &p : this->file_name_entry_format) {
            switch(p.first) {
            case DW_LNCT_path: {
                uint64_t str_offset = this->get_dw_lnct_path(buffer, offset, p.second);
                this->file_names_str.push_back(lineStrTable.get(str_offset));
                break;
            }
            case DW_LNCT_directory_index: {
                uint64_t index = this->get_dw_lcnt_direatory_index(buffer, offset, p.second);
                if(index >= this->directories_str.size())
                    FIG_PANIC("something wrong!");
                this->file_names_directory_name_str.push_back(this->directories_str[index]);
                break;
            }
            default: FIG_PANIC("Unknown DW_LNCT for directory format");
            }
        }
    }
}

uint64_t LineList::get_dw_lnct_path(const char *const buffer, int64_t &offset, uint64_t form) {
    uint64_t res;
    switch(form) {
    case DW_FORM_line_strp: {
        if(this->bitWidth == 64)
            res = read_u64(buffer, offset);
        else
            res = read_u32(buffer, offset);
        break;
    }
    default: FIG_PANIC("Unsupported Form For DW_LNCT_PATH");
    }

    return res;
}

uint64_t LineList::get_dw_lcnt_direatory_index(const char *const buffer, int64_t &offset, uint64_t form) {
    uint64_t res;
    switch(form) {
    case DW_FORM_data1: {
        res = read_u8(buffer, offset);
        break;
    }
    case DW_FORM_data2: {
        res = read_u16(buffer, offset);
        break;
    }
    case DW_FORM_udata: {
        res = read_leb128u(buffer, offset);
        break;
    }
    default: FIG_PANIC("Unsupported Form For DW_LNCT_DIREATORY_INDEX");
    }

    return res;
}