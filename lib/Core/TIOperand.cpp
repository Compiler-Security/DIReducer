#include <capstone/x86.h> // for x86_op_type, x86_reg, x86_op_mem, cs_x...
#include <set>            // for set, _Rb_tree_const_iterator
#include <string>         // for allocator, char_traits, operator+, bas...
#include "CFG/Utils.h"    // for dec_to_hex, x86_op_mem_to_string, x86_...
#include "Core/TIOperand.h" // for TypeInferOP, TypeInferOPFactory, TypeI...
#include "Fig/panic.hpp"

bool operator==(const x86_op_mem &lhs, const x86_op_mem &rhs) {
    bool flag1 = lhs.segment == rhs.segment && lhs.base == rhs.base && lhs.disp == rhs.disp;
    bool flag2 = (lhs.index == rhs.index);
    bool flag3 = (lhs.index == X86_REG_INVALID) ? true : (lhs.scale == rhs.scale);
    return flag1 && flag2 && flag3;
}

namespace SA {

    TIOperand::TIOperand() {
        this->type               = X86_OP_INVALID;
        this->detail.mem.segment = X86_REG_INVALID;
        this->detail.mem.base    = X86_REG_INVALID;
        this->detail.mem.index   = X86_REG_INVALID;
        this->detail.mem.scale   = 1;
        this->detail.mem.disp    = 0;
        this->size               = 0;
    }

    std::string TIOperand::toString() const {
        if(this->type == X86_OP_INVALID)
            FIG_PANIC("this->type == X86_OP_INVALID");
        std::string res;
        switch(this->type) {
        case X86_OP_REG: res = res + CFG::x86_reg_to_string(this->detail.reg); break;
        case X86_OP_IMM: res = res + "0X" + CFG::dec_to_hex(this->detail.imm); break;
        case X86_OP_MEM: res = res + CFG::x86_op_mem_to_string(this->detail.mem); break;
        default: FIG_PANIC("unreached");
        }
        res = res + "@" + std::to_string(this->size);
        return res;
    }

    bool TIOperand::isReg() const {
        return this->type == X86_OP_REG;
    }

    bool TIOperand::isImm() const {
        return this->type == X86_OP_IMM;
    }

    bool TIOperand::isMem() const {
        return this->type == X86_OP_MEM;
    }

    bool TIOperand::isXMMReg() const {
        if(!this->isReg())
            return false;

        if(this->detail.reg >= X86_REG_XMM0 && this->detail.reg <= X86_REG_XMM31)
            return true;
        else
            return false;
    }

    bool TIOperand::isRbpBasedMem() const {
        if(!this->isMem())
            return false;

        if(this->detail.mem.base == X86_REG_RBP)
            return true;
        else
            return false;
    }

    bool TIOperand::isRspBasedMem() const {
        if(!this->isMem())
            return false;

        if(this->detail.mem.base == X86_REG_RSP)
            return true;
        else
            return false;   
    }

    bool TIOperand::isStackMem() const {
        if(!this->isMem())
            return false;

        if(this->detail.mem.base == X86_REG_RSP || this->detail.mem.base == X86_REG_RBP)
            return true;
        else
            return false;
    }

    bool TIOperand::isGlobalMem() const {
        if(!this->isMem())
            return false;

        if(this->detail.mem.base == X86_REG_RIP)
            return true;
        else
            return false;
    }

    /* The Part of TypeInferOPFactory */
    TIOperandFactory::~TIOperandFactory() {
        for(auto ref : this->typeInferOPSet) {
            delete ref;
        }
        this->typeInferOPSet.clear();
        this->immTypeInferOPSet.clear();
        this->regTypeInferOPSet.clear();
        this->memTypeInferOPSet.clear();
    }

    TIOperandRef TIOperandFactory::getTypeInferOPByX86OP(const cs_x86_op &x86OP) {
        if(x86OP.type == X86_OP_INVALID)
            FIG_PANIC("x86OP.type == X86_OP_INVALID");

        switch(x86OP.type) {
        case X86_OP_REG: {
            for(auto it : this->regTypeInferOPSet) {
                if(x86OP.reg == it->detail.reg)
                    return it;
            }
            // create a new regTypeInferOP
            TIOperandRef regTypeInferOP = new TIOperand();
            regTypeInferOP->type        = X86_OP_REG;
            regTypeInferOP->size        = x86OP.size;
            regTypeInferOP->detail.reg  = x86OP.reg;
            this->regTypeInferOPSet.insert(regTypeInferOP);
            this->typeInferOPSet.insert(regTypeInferOP);

            return regTypeInferOP;
        }
        case X86_OP_IMM: {
            for(auto it : this->immTypeInferOPSet) {
                if(x86OP.imm == it->detail.imm && x86OP.size == it->size)
                    return it;
            }
            // create a new immTypeInferOP
            TIOperandRef immTypeInferOP = new TIOperand();
            immTypeInferOP->type        = X86_OP_IMM;
            immTypeInferOP->size        = x86OP.size;
            immTypeInferOP->detail.imm  = x86OP.imm;
            this->immTypeInferOPSet.insert(immTypeInferOP);
            this->typeInferOPSet.insert(immTypeInferOP);

            return immTypeInferOP;
        }
        case X86_OP_MEM: {
            for(auto it : this->memTypeInferOPSet) {
                if(x86OP.mem == it->detail.mem && x86OP.size == it->size)
                    return it;
            }
            // create a new memTypeInferOP
            TIOperandRef memTypeInferOP = new TIOperand();
            memTypeInferOP->type        = X86_OP_MEM;
            memTypeInferOP->size        = x86OP.size;
            memTypeInferOP->detail.mem  = x86OP.mem;
            this->memTypeInferOPSet.insert(memTypeInferOP);
            this->typeInferOPSet.insert(memTypeInferOP);

            return memTypeInferOP;
        }
        default: FIG_PANIC("unreached path!");
        }
    }

    TIOperandRef TIOperandFactory::getRegOPByx86Reg(x86_reg reg) {
        for(auto it : this->regTypeInferOPSet) {
            if(reg == it->detail.reg)
                return it;
        }
        // create a new regTypeInferOP
        TIOperandRef regTypeInferOP = new TIOperand();
        regTypeInferOP->type        = X86_OP_REG;
        regTypeInferOP->size        = 8;
        regTypeInferOP->detail.reg  = reg;
        this->regTypeInferOPSet.insert(regTypeInferOP);
        this->typeInferOPSet.insert(regTypeInferOP);

        return regTypeInferOP;
    }

    bool TIOperandFactory::isXMMReg(x86_reg reg) {
        if(reg >= X86_REG_XMM0 && reg <= X86_REG_XMM31)
            return true;
        else
            return false;
    }

    bool TIOperandFactory::isGlobalMem(x86_op_mem mem) {
        if(mem.base == X86_REG_RIP)
            return true;
        else
            return false;
    }
} // namespace SA
