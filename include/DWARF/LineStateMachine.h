#ifndef DR_LINE_STATE_MACHINE_H
#define DR_LINE_STATE_MACHINE_H

#include "Utils/Buffer.h"
#include "Fig/panic.hpp"
#include "libdwarf/dwarf.h"
#include <nlohmann/json.hpp>
#include <cstdint>

struct LineStateMachine {
    /* Information */

    uint8_t addressSize;
    uint8_t minimun_instruction_length;
    uint8_t maximum_operation_per_instruction;
    uint8_t default_is_stmt;
    int8_t line_base;
    uint8_t line_range;
    uint8_t opcode_base;

    /* Register */

    uint64_t address;
    uint64_t op_index;
    uint64_t file;
    uint64_t line;
    uint64_t column;
    bool is_stmt;
    bool basic_block;
    bool end_sequence;
    bool prologue_end;
    bool epilogue_begin;
    uint64_t isa;
    uint64_t discriminator;

    LineStateMachine() {
        this->reset_registers();
        this->addressSize                       = 0;
        this->minimun_instruction_length        = 0;
        this->maximum_operation_per_instruction = 0;
        this->default_is_stmt                   = false;
        this->line_base                         = 0;
        this->line_range                        = 0;
        this->opcode_base                       = 0;
    }

    nlohmann::json to_json() {
        nlohmann::json content = {
            {"address", this->address},
            {"op_index", this->op_index},
            {"file", this->file},
            {"line", this->column},
            {"column", this->column},
            {"is_stmt", this->is_stmt},
            {"basic_block", this->basic_block},
            {"end_sequence", this->end_sequence},
            {"prologue_end", this->prologue_end},
            {"epilogue_begin", this->epilogue_begin},
            {"isa", this->isa},
            {"discriminator", this->discriminator},
        };

        return content;
    }

    void reset_registers() {
        this->address        = 0;
        this->op_index       = 0;
        this->file           = 1;
        this->line           = 1;
        this->column         = 0;
        this->is_stmt        = false;
        this->basic_block    = false;
        this->end_sequence   = false;
        this->prologue_end   = false;
        this->epilogue_begin = false;
        this->isa            = 0;
        this->discriminator  = 0;
    }

    [[nodiscard("Save Machine State")]] bool operation(const char *const buffer, int64_t &offset, bool &flag_clear) {
        uint8_t opcode = read_u8(buffer, offset);
        if(opcode == 0) {
            /* Extend opcode */
            [[maybe_unused]] uint64_t insn_size = read_leb128u(buffer, offset);
            opcode                              = read_u8(buffer, offset);
            switch(opcode) {
            case DW_LNE_end_sequence: {
                flag_clear = true;
                return true;
            }
            case DW_LNE_set_address: {
                this->op_index = 0;
                if(this->addressSize == 8)
                    this->address = read_u64(buffer, offset);
                else if(this->addressSize == 4)
                    this->address = read_u32(buffer, offset);
                else
                    FIG_PANIC("Unsupported AddressSize: ", static_cast<uint64_t>(this->addressSize));
                break;
            }
            case DW_LNE_set_discriminator: {
                this->discriminator = read_leb128u(buffer, offset);
                break;
            }
            default: FIG_PANIC("Unsupported Extend opcode!");
            }
        } else if(opcode >= this->opcode_base) {
            /* Special opcode */
            this->execute_special_code(opcode);
            return true;
        } else {
            /* Standard opcode */
            switch(opcode) {
            case DW_LNS_copy: {
                this->discriminator  = 0;
                this->basic_block    = false;
                this->prologue_end   = false;
                this->epilogue_begin = false;
                return true;
            }
            case DW_LNS_advance_pc: {
                uint64_t operation_advance = read_leb128u(buffer, offset);
                this->address              = this->address + this->minimun_instruction_length * ((this->op_index + operation_advance) / this->maximum_operation_per_instruction);
                this->op_index             = (this->op_index + operation_advance) % this->maximum_operation_per_instruction;
                break;
            }
            case DW_LNS_advance_line: {
                int64_t line_inc = read_leb128s(buffer, offset);
                this->line       = this->line + line_inc;
                break;
            }
            case DW_LNS_set_file: {
                this->file = read_leb128u(buffer, offset);
                break;
            }
            case DW_LNS_set_column: {
                this->column = read_leb128u(buffer, offset);
                break;
            }
            case DW_LNS_negate_stmt: {
                this->is_stmt = !this->is_stmt;
                break;
            }
            case DW_LNS_set_basic_block: {
                this->basic_block = true;
                break;
            }
            case DW_LNS_const_add_pc: {
                uint64_t adjusted_opcode   = 255 - this->opcode_base;
                uint64_t operation_advance = adjusted_opcode / this->line_range;
                this->address              = this->address + this->minimun_instruction_length * ((this->op_index + operation_advance) / this->maximum_operation_per_instruction);
                this->op_index             = (this->op_index + operation_advance) % this->maximum_operation_per_instruction;
                break;
            }
            case DW_LNS_fixed_advance_pc: {
                uint16_t addr_inc = read_u16(buffer, offset);
                this->address += addr_inc;
                this->op_index = 0;
                break;
            }
            case DW_LNS_set_prologue_end: {
                this->prologue_end = true;
                break;
            }
            case DW_LNS_set_epilogue_begin: {
                this->epilogue_begin = true;
                break;
            }
            case DW_LNS_set_isa: {
                this->isa = read_leb128u(buffer, offset);
                break;
            }
            default: FIG_PANIC("Not a standard opcode!");
            }
        }

        return false;
    }

private:
    void execute_special_code(uint8_t opcode) {
        /* DWARF5.pdf Page 160 */
        /* 1-2 */
        uint64_t adjusted_opcode   = opcode - this->opcode_base;
        uint64_t operation_advance = adjusted_opcode / this->line_range;
        this->line                 = this->line + this->line_base + (adjusted_opcode % this->line_range);
        this->address              = this->address + this->minimun_instruction_length * ((this->op_index + operation_advance) / this->maximum_operation_per_instruction);
        this->op_index             = (this->op_index + operation_advance) % this->maximum_operation_per_instruction;

        /* 4-7 */
        this->basic_block    = false;
        this->prologue_end   = false;
        this->epilogue_begin = false;
        this->discriminator  = 0;
    }
};

#endif