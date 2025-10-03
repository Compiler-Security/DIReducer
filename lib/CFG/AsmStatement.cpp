#include <stdint.h>           // for uint16_t, uint32_t
#include <string>             // for allocator, char_traits, operator+
#include "CFG/AsmStatement.h" // for asm_statement, insnToString
#include "CFG/Utils.h"        // for cs_x86_op_to_string, dec_to_hex

std::string CFG::insnToString(const AsmStatement &insn) {
    std::string res;
    res = "0x" + dec_to_hex(insn.address) + " " + insn.mnemonic + " " + insn.op_str;
    if(insn.detail != nullptr) {
        auto detail = insn.detail->x86;
        res         = res + "{op_id :" + std::to_string(insn.id);
        res         = res + ", op_count:" + std::to_string(static_cast<uint16_t>(detail.op_count));
        for(uint32_t cnt = 0; cnt < detail.op_count; cnt++) {
            res = res + ", Operand" + std::to_string(cnt + 1) + ":{";
            res = res + cs_x86_op_to_string(detail.operands[cnt]);
            res = res + "}";
        }
        res = res + "}";
    }

    return res;
}