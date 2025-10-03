#ifndef TI_TYPECALCULATOR_H
#define TI_TYPECALCULATOR_H

#include <unordered_set>
#include "CFG/Type.h"

namespace SA {
    struct TypeCalculator {
    public:
        std::unordered_set<CFG::VTypePointer> typeSet;

    public:
        TypeCalculator() = default;
        ~TypeCalculator();

    public:
        CFG::VTypePointer getUnionVType(CFG::VTypePointer vType1, CFG::VTypePointer vType2);
        CFG::VTypePointer getPointerVType(CFG::VTypePointer vType);
        CFG::VTypePointer getObjectVType(CFG::VTypePointer vType);
    };
} // namespace SA

#endif