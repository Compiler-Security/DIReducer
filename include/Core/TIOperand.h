#ifndef TI_OPERAND_H
#define TI_OPERAND_H

#include <cstdint>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include "capstone/x86.h"

bool operator==(const x86_op_mem &lhs, const x86_op_mem &rhs);

namespace SA {
    struct TIOperand;
    struct TIOperandFactory;
    using TIOperandRef = TIOperand *;
} // namespace SA

struct SA::TIOperand {
    friend struct SA::TIOperandFactory;

public:
    x86_op_type type;
    union {
        x86_reg reg;
        int64_t imm;
        x86_op_mem mem;
    } detail;
    uint8_t size;

public:
    std::string toString() const;
    bool isReg() const;
    bool isImm() const;
    bool isMem() const;
    bool isXMMReg() const;
    bool isRbpBasedMem() const;
    bool isRspBasedMem() const;
    bool isStackMem() const;
    bool isGlobalMem() const;

private:
    TIOperand();
};

struct SA::TIOperandFactory {
private:
    std::set<TIOperandRef> immTypeInferOPSet;
    std::set<TIOperandRef> regTypeInferOPSet;
    std::set<TIOperandRef> memTypeInferOPSet;
    std::set<TIOperandRef> typeInferOPSet;

public:
    TIOperandFactory() = default;
    ~TIOperandFactory();
    TIOperandRef getTypeInferOPByX86OP(const cs_x86_op &x86OP);
    TIOperandRef getRegOPByx86Reg(x86_reg reg);
    static bool isXMMReg(x86_reg reg);
    static bool isGlobalMem(x86_op_mem mem);
};

#endif