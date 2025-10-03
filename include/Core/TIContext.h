#ifndef TI_CONTEXT_H
#define TI_CONTEXT_H

#include <unordered_map>
#include <string>
#include "TIOperand.h"
#include "CFG/Type.h"

namespace SA {
    struct TIContext : public std::unordered_map<TIOperandRef, CFG::VTypePointer> {
        std::string toString() const;
    };
} // namespace SA

#endif