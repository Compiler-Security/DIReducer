#include <libdwarf/dwarf.h> // for DW_OP_GNU_addr_index, DW_OP_GNU_con...
#include <stdint.h>         // for int16_t, int64_t, uint8_t
#include <cstring>          // for memcpy
#include <iostream>         // for operator<<, basic_ios, basic_ostream
#include <memory>           // for shared_ptr
#include "DWARF/DWARFOP.h"  // for dwarf_op
#include "Utils/Buffer.h"   // for common_buffer
#include "Fig/panic.hpp"
#include "Utils/ToStr.h" // for dw_op_to_str

void DWARFOP::print() {
    std::cout << dw_op_to_str(this->operation);
}

void DWARFOP::write(char *buffer, int64_t &offset) {
    write_u8(this->operation, buffer, offset);
    switch(this->operation) {
    case DW_OP_addr: {
        if(this->operandSize1 == 8)
            write_u64(this->operand1.u, buffer, offset);
        else
            write_u32(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_deref: { /* No Operand */ break;
    }
    case DW_OP_const1u: {
        write_u8(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_const1s: {
        write_i8(this->operand1.s, buffer, offset);
        break;
    }
    case DW_OP_const2u: {
        write_u16(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_const2s: {
        write_i16(this->operand1.s, buffer, offset);
        break;
    }
    case DW_OP_const4u: {
        write_u32(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_const4s: {
        write_i32(this->operand1.s, buffer, offset);
        break;
    }
    case DW_OP_const8u: {
        write_u64(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_const8s: {
        write_i64(this->operand1.s, buffer, offset);
        break;
    }
    case DW_OP_constu: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_consts: {
        write_leb128s_fixed_length(this->operand1.s, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_dup: { /* No Operand */ break;
    }
    case DW_OP_drop: { /* No Operand */ break;
    }
    case DW_OP_over: { /* No Operand */ break;
    }
    case DW_OP_pick: {
        write_u8(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_swap: { /* No Operand */ break;
    }
    case DW_OP_rot: { /* No Operand */ break;
    }
    case DW_OP_xderef: { /* No Operand */ break;
    }
    case DW_OP_abs: { /* No Operand */ break;
    }
    case DW_OP_and: { /* No Operand */ break;
    }
    case DW_OP_div: { /* No Operand */ break;
    }
    case DW_OP_minus: { /* No Operand */ break;
    }
    case DW_OP_mod: { /* No Operand */ break;
    }
    case DW_OP_mul: { /* No Operand */ break;
    }
    case DW_OP_neg: { /* No Operand */ break;
    }
    case DW_OP_not: { /* No Operand */ break;
    }
    case DW_OP_or: { /* No Operand */ break;
    }
    case DW_OP_plus: { /* No Operand */ break;
    }
    case DW_OP_plus_uconst: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_shl: { /* No Operand */ break;
    }
    case DW_OP_shr: { /* No Operand */ break;
    }
    case DW_OP_shra: { /* No Operand */ break;
    }
    case DW_OP_xor: { /* No Operand */ break;
    }
    case DW_OP_bra: {
        write_i16(this->operand1.s, buffer, offset);
        break;
    }
    case DW_OP_eq: { /* No Operand */ break;
    }
    case DW_OP_ge: { /* No Operand */ break;
    }
    case DW_OP_gt: { /* No Operand */ break;
    }
    case DW_OP_le: { /* No Operand */ break;
    }
    case DW_OP_lt: { /* No Operand */ break;
    }
    case DW_OP_ne: { /* No Operand */ break;
    }
    case DW_OP_skip: {
        write_i16(this->operand1.s, buffer, offset);
        break;
    }
    case DW_OP_lit0:  /* No Operand */
    case DW_OP_lit1:  /* No Operand */
    case DW_OP_lit2:  /* No Operand */
    case DW_OP_lit3:  /* No Operand */
    case DW_OP_lit4:  /* No Operand */
    case DW_OP_lit5:  /* No Operand */
    case DW_OP_lit6:  /* No Operand */
    case DW_OP_lit7:  /* No Operand */
    case DW_OP_lit8:  /* No Operand */
    case DW_OP_lit9:  /* No Operand */
    case DW_OP_lit10: /* No Operand */
    case DW_OP_lit11: /* No Operand */
    case DW_OP_lit12: /* No Operand */
    case DW_OP_lit13: /* No Operand */
    case DW_OP_lit14: /* No Operand */
    case DW_OP_lit15: /* No Operand */
    case DW_OP_lit16: /* No Operand */
    case DW_OP_lit17: /* No Operand */
    case DW_OP_lit18: /* No Operand */
    case DW_OP_lit19: /* No Operand */
    case DW_OP_lit20: /* No Operand */
    case DW_OP_lit21: /* No Operand */
    case DW_OP_lit22: /* No Operand */
    case DW_OP_lit23: /* No Operand */
    case DW_OP_lit24: /* No Operand */
    case DW_OP_lit25: /* No Operand */
    case DW_OP_lit26: /* No Operand */
    case DW_OP_lit27: /* No Operand */
    case DW_OP_lit28: /* No Operand */
    case DW_OP_lit29: /* No Operand */
    case DW_OP_lit30: /* No Operand */
    case DW_OP_lit31: /* No Operand */
    case DW_OP_reg0:  /* No Operand */
    case DW_OP_reg1:  /* No Operand */
    case DW_OP_reg2:  /* No Operand */
    case DW_OP_reg3:  /* No Operand */
    case DW_OP_reg4:  /* No Operand */
    case DW_OP_reg5:  /* No Operand */
    case DW_OP_reg6:  /* No Operand */
    case DW_OP_reg7:  /* No Operand */
    case DW_OP_reg8:  /* No Operand */
    case DW_OP_reg9:  /* No Operand */
    case DW_OP_reg10: /* No Operand */
    case DW_OP_reg11: /* No Operand */
    case DW_OP_reg12: /* No Operand */
    case DW_OP_reg13: /* No Operand */
    case DW_OP_reg14: /* No Operand */
    case DW_OP_reg15: /* No Operand */
    case DW_OP_reg16: /* No Operand */
    case DW_OP_reg17: /* No Operand */
    case DW_OP_reg18: /* No Operand */
    case DW_OP_reg19: /* No Operand */
    case DW_OP_reg20: /* No Operand */
    case DW_OP_reg21: /* No Operand */
    case DW_OP_reg22: /* No Operand */
    case DW_OP_reg23: /* No Operand */
    case DW_OP_reg24: /* No Operand */
    case DW_OP_reg25: /* No Operand */
    case DW_OP_reg26: /* No Operand */
    case DW_OP_reg27: /* No Operand */
    case DW_OP_reg28: /* No Operand */
    case DW_OP_reg29: /* No Operand */
    case DW_OP_reg30: /* No Operand */
    case DW_OP_reg31: {
        break;
    }
    case DW_OP_breg0:  /* DW_OP_breg */
    case DW_OP_breg1:  /* DW_OP_breg */
    case DW_OP_breg2:  /* DW_OP_breg */
    case DW_OP_breg3:  /* DW_OP_breg */
    case DW_OP_breg4:  /* DW_OP_breg */
    case DW_OP_breg5:  /* DW_OP_breg */
    case DW_OP_breg6:  /* DW_OP_breg */
    case DW_OP_breg7:  /* DW_OP_breg */
    case DW_OP_breg8:  /* DW_OP_breg */
    case DW_OP_breg9:  /* DW_OP_breg */
    case DW_OP_breg10: /* DW_OP_breg */
    case DW_OP_breg11: /* DW_OP_breg */
    case DW_OP_breg12: /* DW_OP_breg */
    case DW_OP_breg13: /* DW_OP_breg */
    case DW_OP_breg14: /* DW_OP_breg */
    case DW_OP_breg15: /* DW_OP_breg */
    case DW_OP_breg16: /* DW_OP_breg */
    case DW_OP_breg17: /* DW_OP_breg */
    case DW_OP_breg18: /* DW_OP_breg */
    case DW_OP_breg19: /* DW_OP_breg */
    case DW_OP_breg20: /* DW_OP_breg */
    case DW_OP_breg21: /* DW_OP_breg */
    case DW_OP_breg22: /* DW_OP_breg */
    case DW_OP_breg23: /* DW_OP_breg */
    case DW_OP_breg24: /* DW_OP_breg */
    case DW_OP_breg25: /* DW_OP_breg */
    case DW_OP_breg26: /* DW_OP_breg */
    case DW_OP_breg27: /* DW_OP_breg */
    case DW_OP_breg28: /* DW_OP_breg */
    case DW_OP_breg29: /* DW_OP_breg */
    case DW_OP_breg30: /* DW_OP_breg */
    case DW_OP_breg31: {
        write_leb128s_fixed_length(this->operand1.s, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_regx: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_fbreg: {
        write_leb128s_fixed_length(this->operand1.s, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_bregx: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        write_leb128s_fixed_length(this->operand2.s, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_piece: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_deref_size: {
        write_u8(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_xderef_size: {
        write_u8(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_nop: { /* No Operand */ break;
    }
    case DW_OP_push_object_address: { /* No Operand */ break;
    }
    case DW_OP_call2: {
        write_u16(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_call4: {
        write_u32(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_call_ref: {
        if(this->operandSize1 == 8)
            write_u64(this->operand1.u, buffer, offset);
        else
            write_u32(this->operand1.u, buffer, offset);
    }
    case DW_OP_form_tls_address: { /* No Operand */ break;
    }
    case DW_OP_call_frame_cfa: { /* No Operand */ break;
    }
    case DW_OP_bit_piece: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        write_leb128u_fixed_length(this->operand2.u, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_implicit_value: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        std::memcpy(buffer + offset, this->blockBuffer.buffer.get(), this->blockBuffer.size);
        offset += this->blockBuffer.size;
        break;
    }
    case DW_OP_stack_value: { /* No Operand */ break;
    }
    case DW_OP_implicit_pointer: {
        if(this->operandSize1 == 8)
            write_u64(this->operand1.u, buffer, offset);
        else
            write_u32(this->operand1.u, buffer, offset);

        write_leb128s_fixed_length(this->operand2.s, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_addrx: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_constx: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_entry_value: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        std::memcpy(buffer + offset, this->blockBuffer.buffer.get(), this->blockBuffer.size);
        offset += this->blockBuffer.size;
        break;
    }
    case DW_OP_const_type: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        write_u8(this->operand2.u, buffer, offset);
        std::memcpy(buffer + offset, this->blockBuffer.buffer.get(), this->blockBuffer.size);
        offset += this->blockBuffer.size;
        break;
    }
    case DW_OP_regval_type: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        write_leb128u_fixed_length(this->operand2.u, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_deref_type: {
        write_u8(this->operand1.u, buffer, offset);
        write_leb128u_fixed_length(this->operand2.u, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_xderef_type: {
        write_u8(this->operand1.u, buffer, offset);
        write_leb128u_fixed_length(this->operand2.u, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_convert: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_reinterpret: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_GNU_push_tls_address: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_WASM_location: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_WASM_location_int: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    // case  DW_OP_lo_user           : {} /* confilct */
    case DW_OP_LLVM_form_aspace_address: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_push_lane: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_offset: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_offset_uconst: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_bit_offset: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_call_frame_entry_reg: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_undefined: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_aspace_bregx: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_aspace_implicit_pointer: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_piece_end: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_extend: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_select_bit_piece: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    // case  DW_OP_HP_unknown             : {} /* confilct */
    // case  DW_OP_HP_is_value            : {} /* confilct */
    // case  DW_OP_HP_fltconst4           : {} /* confilct */
    // case  DW_OP_HP_fltconst8           : {} /* confilct */
    // case  DW_OP_HP_mod_range           : {} /* confilct */
    // case  DW_OP_HP_unmod_range         : {} /* confilct */
    // case  DW_OP_HP_tls                 : {} /* confilct */
    // case  DW_OP_INTEL_bit_piece        : {} /* confilct */
    case DW_OP_GNU_uninit: { /* No Operand */ break;
    }
    // case  DW_OP_APPLE_uninit      : {} /* confilct */
    case DW_OP_GNU_encoded_addr: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_GNU_implicit_pointer: {
        if(this->operandSize1 == 8)
            write_u64(this->operand1.u, buffer, offset);
        else
            write_u32(this->operand1.u, buffer, offset);

        write_leb128s_fixed_length(this->operand2.s, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_GNU_entry_value: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        std::memcpy(buffer + offset, this->blockBuffer.buffer.get(), this->blockBuffer.size);
        offset += this->blockBuffer.size;
        break;
    }
    case DW_OP_GNU_const_type: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        write_u8(this->operand2.u, buffer, offset);
        std::memcpy(buffer + offset, this->blockBuffer.buffer.get(), this->blockBuffer.size);
        offset += this->blockBuffer.size;
        break;
    }
    case DW_OP_GNU_regval_type: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        write_leb128u_fixed_length(this->operand2.u, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_GNU_deref_type: {
        write_u8(this->operand1.u, buffer, offset);
        write_leb128u_fixed_length(this->operand2.u, buffer, offset, this->operandSize2);
        break;
    }
    case DW_OP_GNU_convert: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_GNU_reinterpret: {
        write_leb128u_fixed_length(this->operand1.u, buffer, offset, this->operandSize1);
        break;
    }
    case DW_OP_GNU_parameter_ref: {
        write_u32(this->operand1.u, buffer, offset);
        break;
    }
    case DW_OP_GNU_addr_index: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_GNU_const_index: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_GNU_variable_value: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_PGI_omp_thread_num: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    case DW_OP_hi_user: FIG_PANIC("Unsupported DW_OP", dw_op_to_str(this->operation)); break;
    default: FIG_PANIC("Unsupported DW_OP: ", static_cast<int16_t>(this->operation)); break;
    }
}

void DWARFOP::init(const char *buffer, int64_t &offset, uint8_t bitWidth, uint8_t addressSize) {
    this->operation = read_u8(buffer, offset);
    switch(this->operation) {
    case DW_OP_addr: {
        this->operandSize1 = addressSize;
        if(addressSize == 8)
            this->operand1.u = read_u64(buffer, offset);
        else
            this->operand1.u = read_u32(buffer, offset);
        break;
    }
    case DW_OP_deref: { /* No Operand */ break;
    }
    case DW_OP_const1u: {
        this->operandSize1 = 1;
        this->operand1.u   = read_u8(buffer, offset);
        break;
    }
    case DW_OP_const1s: {
        this->operandSize1 = 1;
        this->operand1.s   = read_i8(buffer, offset);
        break;
    }
    case DW_OP_const2u: {
        this->operandSize1 = 2;
        this->operand1.u   = read_u16(buffer, offset);
        break;
    }
    case DW_OP_const2s: {
        this->operandSize1 = 2;
        this->operand1.s   = read_i16(buffer, offset);
        break;
    }
    case DW_OP_const4u: {
        this->operandSize1 = 4;
        this->operand1.u   = read_u32(buffer, offset);
        break;
    }
    case DW_OP_const4s: {
        this->operandSize1 = 4;
        this->operand1.s   = read_i32(buffer, offset);
        break;
    }
    case DW_OP_const8u: {
        this->operandSize1 = 8;
        this->operand1.u   = read_u64(buffer, offset);
        break;
    }
    case DW_OP_const8s: {
        this->operandSize1 = 8;
        this->operand1.s   = read_i64(buffer, offset);
        break;
    }
    case DW_OP_constu: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_consts: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.s   = read_leb128s(buffer, offset);
        break;
    }
    case DW_OP_dup: { /* No Operand */ break;
    }
    case DW_OP_drop: { /* No Operand */ break;
    }
    case DW_OP_over: { /* No Operand */ break;
    }
    case DW_OP_pick: {
        this->operandSize1 = 1;
        this->operand1.u   = read_u8(buffer, offset);
        break;
    }
    case DW_OP_swap: { /* No Operand */ break;
    }
    case DW_OP_rot: { /* No Operand */ break;
    }
    case DW_OP_xderef: { /* No Operand */ break;
    }
    case DW_OP_abs: { /* No Operand */ break;
    }
    case DW_OP_and: { /* No Operand */ break;
    }
    case DW_OP_div: { /* No Operand */ break;
    }
    case DW_OP_minus: { /* No Operand */ break;
    }
    case DW_OP_mod: { /* No Operand */ break;
    }
    case DW_OP_mul: { /* No Operand */ break;
    }
    case DW_OP_neg: { /* No Operand */ break;
    }
    case DW_OP_not: { /* No Operand */ break;
    }
    case DW_OP_or: { /* No Operand */ break;
    }
    case DW_OP_plus: { /* No Operand */ break;
    }
    case DW_OP_plus_uconst: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_shl: { /* No Operand */ break;
    }
    case DW_OP_shr: { /* No Operand */ break;
    }
    case DW_OP_shra: { /* No Operand */ break;
    }
    case DW_OP_xor: { /* No Operand */ break;
    }
    case DW_OP_bra: {
        this->operandSize1 = 2;
        this->operand1.s   = read_i16(buffer, offset);
        break;
    }
    case DW_OP_eq: { /* No Operand */ break;
    }
    case DW_OP_ge: { /* No Operand */ break;
    }
    case DW_OP_gt: { /* No Operand */ break;
    }
    case DW_OP_le: { /* No Operand */ break;
    }
    case DW_OP_lt: { /* No Operand */ break;
    }
    case DW_OP_ne: { /* No Operand */ break;
    }
    case DW_OP_skip: {
        this->operandSize1 = 2;
        this->operand1.s   = read_i16(buffer, offset);
        break;
    }
    case DW_OP_lit0: { /* No Operand */ break;
    }
    case DW_OP_lit1: { /* No Operand */ break;
    }
    case DW_OP_lit2: { /* No Operand */ break;
    }
    case DW_OP_lit3: { /* No Operand */ break;
    }
    case DW_OP_lit4: { /* No Operand */ break;
    }
    case DW_OP_lit5: { /* No Operand */ break;
    }
    case DW_OP_lit6: { /* No Operand */ break;
    }
    case DW_OP_lit7: { /* No Operand */ break;
    }
    case DW_OP_lit8: { /* No Operand */ break;
    }
    case DW_OP_lit9: { /* No Operand */ break;
    }
    case DW_OP_lit10: { /* No Operand */ break;
    }
    case DW_OP_lit11: { /* No Operand */ break;
    }
    case DW_OP_lit12: { /* No Operand */ break;
    }
    case DW_OP_lit13: { /* No Operand */ break;
    }
    case DW_OP_lit14: { /* No Operand */ break;
    }
    case DW_OP_lit15: { /* No Operand */ break;
    }
    case DW_OP_lit16: { /* No Operand */ break;
    }
    case DW_OP_lit17: { /* No Operand */ break;
    }
    case DW_OP_lit18: { /* No Operand */ break;
    }
    case DW_OP_lit19: { /* No Operand */ break;
    }
    case DW_OP_lit20: { /* No Operand */ break;
    }
    case DW_OP_lit21: { /* No Operand */ break;
    }
    case DW_OP_lit22: { /* No Operand */ break;
    }
    case DW_OP_lit23: { /* No Operand */ break;
    }
    case DW_OP_lit24: { /* No Operand */ break;
    }
    case DW_OP_lit25: { /* No Operand */ break;
    }
    case DW_OP_lit26: { /* No Operand */ break;
    }
    case DW_OP_lit27: { /* No Operand */ break;
    }
    case DW_OP_lit28: { /* No Operand */ break;
    }
    case DW_OP_lit29: { /* No Operand */ break;
    }
    case DW_OP_lit30: { /* No Operand */ break;
    }
    case DW_OP_lit31: { /* No Operand */ break;
    }
    case DW_OP_reg0: { /* No Operand */ break;
    }
    case DW_OP_reg1: { /* No Operand */ break;
    }
    case DW_OP_reg2: { /* No Operand */ break;
    }
    case DW_OP_reg3: { /* No Operand */ break;
    }
    case DW_OP_reg4: { /* No Operand */ break;
    }
    case DW_OP_reg5: { /* No Operand */ break;
    }
    case DW_OP_reg6: { /* No Operand */ break;
    }
    case DW_OP_reg7: { /* No Operand */ break;
    }
    case DW_OP_reg8: { /* No Operand */ break;
    }
    case DW_OP_reg9: { /* No Operand */ break;
    }
    case DW_OP_reg10: { /* No Operand */ break;
    }
    case DW_OP_reg11: { /* No Operand */ break;
    }
    case DW_OP_reg12: { /* No Operand */ break;
    }
    case DW_OP_reg13: { /* No Operand */ break;
    }
    case DW_OP_reg14: { /* No Operand */ break;
    }
    case DW_OP_reg15: { /* No Operand */ break;
    }
    case DW_OP_reg16: { /* No Operand */ break;
    }
    case DW_OP_reg17: { /* No Operand */ break;
    }
    case DW_OP_reg18: { /* No Operand */ break;
    }
    case DW_OP_reg19: { /* No Operand */ break;
    }
    case DW_OP_reg20: { /* No Operand */ break;
    }
    case DW_OP_reg21: { /* No Operand */ break;
    }
    case DW_OP_reg22: { /* No Operand */ break;
    }
    case DW_OP_reg23: { /* No Operand */ break;
    }
    case DW_OP_reg24: { /* No Operand */ break;
    }
    case DW_OP_reg25: { /* No Operand */ break;
    }
    case DW_OP_reg26: { /* No Operand */ break;
    }
    case DW_OP_reg27: { /* No Operand */ break;
    }
    case DW_OP_reg28: { /* No Operand */ break;
    }
    case DW_OP_reg29: { /* No Operand */ break;
    }
    case DW_OP_reg30: { /* No Operand */ break;
    }
    case DW_OP_reg31: { /* No Operand */ break;
    }
    case DW_OP_breg0:  /* DW_OP_breg */
    case DW_OP_breg1:  /* DW_OP_breg */
    case DW_OP_breg2:  /* DW_OP_breg */
    case DW_OP_breg3:  /* DW_OP_breg */
    case DW_OP_breg4:  /* DW_OP_breg */
    case DW_OP_breg5:  /* DW_OP_breg */
    case DW_OP_breg6:  /* DW_OP_breg */
    case DW_OP_breg7:  /* DW_OP_breg */
    case DW_OP_breg8:  /* DW_OP_breg */
    case DW_OP_breg9:  /* DW_OP_breg */
    case DW_OP_breg10: /* DW_OP_breg */
    case DW_OP_breg11: /* DW_OP_breg */
    case DW_OP_breg12: /* DW_OP_breg */
    case DW_OP_breg13: /* DW_OP_breg */
    case DW_OP_breg14: /* DW_OP_breg */
    case DW_OP_breg15: /* DW_OP_breg */
    case DW_OP_breg16: /* DW_OP_breg */
    case DW_OP_breg17: /* DW_OP_breg */
    case DW_OP_breg18: /* DW_OP_breg */
    case DW_OP_breg19: /* DW_OP_breg */
    case DW_OP_breg20: /* DW_OP_breg */
    case DW_OP_breg21: /* DW_OP_breg */
    case DW_OP_breg22: /* DW_OP_breg */
    case DW_OP_breg23: /* DW_OP_breg */
    case DW_OP_breg24: /* DW_OP_breg */
    case DW_OP_breg25: /* DW_OP_breg */
    case DW_OP_breg26: /* DW_OP_breg */
    case DW_OP_breg27: /* DW_OP_breg */
    case DW_OP_breg28: /* DW_OP_breg */
    case DW_OP_breg29: /* DW_OP_breg */
    case DW_OP_breg30: /* DW_OP_breg */
    case DW_OP_breg31: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.s   = read_leb128s(buffer, offset);
        break;
    }
    case DW_OP_regx: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_fbreg: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.s   = read_leb128s(buffer, offset);
        break;
    }
    case DW_OP_bregx: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.s   = read_leb128s(buffer, offset);
        break;
    }
    case DW_OP_piece: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_deref_size: {
        this->operandSize1 = 1;
        this->operand1.u   = read_u8(buffer, offset);
        break;
    }
    case DW_OP_xderef_size: {
        this->operandSize1 = 1;
        this->operand1.u   = read_u8(buffer, offset);
        break;
    }
    case DW_OP_nop: { /* No Operand */ break;
    }
    case DW_OP_push_object_address: { /* No Operand */ break;
    }
    case DW_OP_call2: {
        this->operandSize1 = 2;
        this->operand1.u   = read_u16(buffer, offset);
        break;
    }
    case DW_OP_call4: {
        this->operandSize1 = 4;
        this->operand1.u   = read_u32(buffer, offset);
        break;
    }
    case DW_OP_call_ref: {
        this->operandSize1 = bitWidth / 8;
        if(bitWidth == 64)
            this->operand1.u = read_u64(buffer, offset);
        else
            this->operand1.u = read_u32(buffer, offset);
        break;
    }
    case DW_OP_form_tls_address: { /* No Operand */ break;
    }
    case DW_OP_call_frame_cfa: { /* No Operand */ break;
    }
    case DW_OP_bit_piece: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_implicit_value: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->blockBuffer.init(new char[this->operand1.u], this->operand1.u);
        std::memcpy(this->blockBuffer.buffer.get(), buffer + offset, this->operand1.u);
        offset += this->operand1.u;
        break;
    }
    case DW_OP_stack_value: { /* No Operand */ break;
    }
    case DW_OP_implicit_pointer: {
        this->operandSize1 = bitWidth / 8;
        if(bitWidth == 64)
            this->operand1.u = read_u64(buffer, offset);
        else
            this->operand1.u = read_u32(buffer, offset);

        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.s   = read_leb128s(buffer, offset);
        break;
    }
    case DW_OP_addrx: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_constx: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_entry_value: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->blockBuffer.init(new char[this->operand1.u], this->operand1.u);
        std::memcpy(this->blockBuffer.buffer.get(), buffer + offset, this->operand1.u);
        offset += this->operand1.u;
        break;
    }
    case DW_OP_const_type: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->operandSize2 = 1;
        this->operand2.u   = read_u8(buffer, offset);
        this->blockBuffer.init(new char[this->operand2.u], this->operand2.u);
        std::memcpy(this->blockBuffer.buffer.get(), buffer + offset, this->operand2.u);
        offset += this->operand2.u;
        break;
    }
    case DW_OP_regval_type: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_deref_type: {
        this->operandSize1 = 1;
        this->operand1.u   = read_u8(buffer, offset);
        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_xderef_type: {
        this->operandSize1 = 1;
        this->operand1.u   = read_u8(buffer, offset);
        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_convert: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_reinterpret: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_GNU_push_tls_address: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_WASM_location: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_WASM_location_int: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    // case  DW_OP_lo_user           : {} /* confilct */
    case DW_OP_LLVM_form_aspace_address: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_push_lane: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_offset: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_offset_uconst: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_bit_offset: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_call_frame_entry_reg: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_undefined: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_aspace_bregx: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_aspace_implicit_pointer: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_piece_end: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_extend: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_LLVM_select_bit_piece: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    // case  DW_OP_HP_unknown             : {} /* confilct */
    // case  DW_OP_HP_is_value            : {} /* confilct */
    // case  DW_OP_HP_fltconst4           : {} /* confilct */
    // case  DW_OP_HP_fltconst8           : {} /* confilct */
    // case  DW_OP_HP_mod_range           : {} /* confilct */
    // case  DW_OP_HP_unmod_range         : {} /* confilct */
    // case  DW_OP_HP_tls                 : {} /* confilct */
    // case  DW_OP_INTEL_bit_piece        : {} /* confilct */
    case DW_OP_GNU_uninit: { /* No Operand */ break;
    }
    // case  DW_OP_APPLE_uninit      : {} /* confilct */
    case DW_OP_GNU_encoded_addr: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_GNU_implicit_pointer: {
        this->operandSize1 = bitWidth / 8;
        if(bitWidth == 64)
            this->operand1.u = read_u64(buffer, offset);
        else
            this->operand1.u = read_u32(buffer, offset);

        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.s   = read_leb128s(buffer, offset);
        break;
    }
    case DW_OP_GNU_entry_value: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->blockBuffer.init(new char[this->operand1.u], this->operand1.u);
        std::memcpy(this->blockBuffer.buffer.get(), buffer + offset, this->operand1.u);
        offset += this->operand1.u;
        break;
    }
    case DW_OP_GNU_const_type: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->operandSize2 = 1;
        this->operand2.u   = read_u8(buffer, offset);
        this->blockBuffer.init(new char[this->operand2.u], this->operand2.u);
        std::memcpy(this->blockBuffer.buffer.get(), buffer + offset, this->operand2.u);
        offset += this->operand2.u;
        break;
    }
    case DW_OP_GNU_regval_type: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_GNU_deref_type: {
        this->operandSize1 = 1;
        this->operand1.u   = read_u8(buffer, offset);
        this->operandSize2 = get_length_leb128(buffer, offset);
        this->operand2.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_GNU_convert: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_GNU_reinterpret: {
        this->operandSize1 = get_length_leb128(buffer, offset);
        this->operand1.u   = read_leb128u(buffer, offset);
        break;
    }
    case DW_OP_GNU_parameter_ref: {
        this->operandSize1 = 4;
        this->operand1.u   = read_u32(buffer, offset);
        break;
    }
    case DW_OP_GNU_addr_index: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_GNU_const_index: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_GNU_variable_value: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_PGI_omp_thread_num: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    case DW_OP_hi_user: FIG_PANIC("Unsupported DW_OP, ", dw_op_to_str(this->operation)); break;
    default: FIG_PANIC("Unsupported DW_OP: ", static_cast<int16_t>(this->operation)); break;
    }
}