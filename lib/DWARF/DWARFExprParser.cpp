#include "DWARF/DWARFExprParser.h"
#include <iostream>          // for operator<<, basic_ios, basic_ostream, cout
#include <utility>           // for pair
#include "DWARF/DWARFOP.h"  // for dwarf_op
#include "Fig/panic.hpp"     // for FIG_PANIC
#include "Utils/Common.h"  // for dec_to_hex
#include "libdwarf/dwarf.h"  // for DW_OP_GNU_const_type, DW_OP_GNU_deref_type

DWARFExprParser::DWARFExprParser(const char *buffer, int64_t exprSize, uint8_t bitWidth, uint8_t addressSize) {
    this->init(buffer, exprSize, bitWidth, addressSize);
}

void DWARFExprParser::init(const char *buffer, int64_t exprSize, uint8_t bitWidth, uint8_t addressSize) {
    if(bitWidth != 32 && bitWidth != 64)
        FIG_PANIC("Unsupported bitWidth: ", bitWidth);
    if(addressSize != 4 && addressSize != 8)
        FIG_PANIC("Unsupported addressSize: ", addressSize);

    this->bitWidth    = bitWidth;
    this->addressSize = addressSize;

    int64_t offset = 0;
    while(offset < exprSize) {
        this->op_list.emplace_back(buffer, offset, this->bitWidth, this->addressSize);
    }
}

void DWARFExprParser::print() {
    for(auto &op : this->op_list) {
        op.print();
        std::cout << " ";
    }
}

void DWARFExprParser::clear() {
    this->op_list.clear();
}

void DWARFExprParser::fix_reference_to_die(
    const std::unordered_map<int64_t, int64_t> &GlobalOffsetMap,
    const std::unordered_map<int64_t, int64_t> &DIEOffsetMap) {
    for(auto &op : this->op_list) {
        switch(op.operation) {
        case DW_OP_call2:
        case DW_OP_call4:
        case DW_OP_call_ref:
        case DW_OP_GNU_const_type: /* GNU extension */
        case DW_OP_const_type: {
            if(DIEOffsetMap.find(op.operand1.u) == DIEOffsetMap.end())
                FIG_PANIC("Old Inner Offset: ", op.operand1.u, " not found in DIEOffsetMap", "\n");
            int64_t newInnerOffset = DIEOffsetMap.find(op.operand1.u)->second;
            op.operand1.u          = newInnerOffset;
            break;
        }
        case DW_OP_convert:
        case DW_OP_reinterpret: {
            if(op.operand1.u == 0)
                break;
            if(DIEOffsetMap.find(op.operand1.u) == DIEOffsetMap.end())
                FIG_PANIC("Old Inner Offset: ", dec_to_hex(op.operand1.u), " not found in DIEOffsetMap", "\n");
            int64_t newInnerOffset = DIEOffsetMap.find(op.operand1.u)->second;
            op.operand1.u          = newInnerOffset;
            break;
        }
        case DW_OP_implicit_pointer: {
            if(GlobalOffsetMap.find(op.operand1.u) == GlobalOffsetMap.end())
                FIG_PANIC("Old Global Offset: ", op.operand1.u, " not found in GlobalOffsetMap", "\n");
            int64_t newGlobalOffset = GlobalOffsetMap.find(op.operand1.u)->second;
            op.operand1.u           = newGlobalOffset;
            break;
        }
        case DW_OP_regval_type:
        case DW_OP_GNU_regval_type: /* GNU extension */
        case DW_OP_deref_type:
        case DW_OP_GNU_deref_type: /* GNU extension */
        case DW_OP_xderef_type: {
            if(DIEOffsetMap.find(op.operand2.u) == DIEOffsetMap.end())
                FIG_PANIC("Old Inner Offset: ", op.operand2.u, " not found in DIEOffsetMap", "\n");
            int64_t newInnerOffset = DIEOffsetMap.find(op.operand2.u)->second;
            op.operand2.u          = newInnerOffset;
            break;
        }
        case DW_OP_GNU_parameter_ref: {
            if(DIEOffsetMap.find(op.operand1.u) == DIEOffsetMap.end())
                FIG_PANIC("Old Inner Offset: ", op.operand2.u, " not found in DIEOffsetMap", "\n");
            int64_t newInnerOffset = DIEOffsetMap.find(op.operand1.u)->second;
            op.operand1.u          = newInnerOffset;
        }
        default: break;
        }
    }
}

void DWARFExprParser::write(char *buffer, int64_t &offset) {
    for(auto &op : this->op_list) {
        op.write(buffer, offset);
    }
}