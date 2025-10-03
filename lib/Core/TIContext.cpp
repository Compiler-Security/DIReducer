#include "Core/TIContext.h"
#include <utility>        // for pair
#include "Core/TIOperand.h" // for TIOperand

namespace SA {
    std::string TIContext::toString() const {
        std::string contextStr;
        for(auto &it : *this) {
            contextStr = contextStr + "{" + it.first->toString() + " : " + it.second->name + "}";
        }
        return contextStr;
    }
} // namespace SA