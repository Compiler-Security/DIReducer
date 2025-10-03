#include "Core/TIConfigure.h"

namespace SA {
    TIConfigure::TIConfigure() {
        this->enableGlobalVariablAnalysis   = false;
        this->enableInterProceduralAnalysis = false;
        this->enableMemOPCalculatedAnalysis = false;
        this->enableSavingRegCompensation   = false;
        this->enableUsingUnknownVar        = false;
    }
} // namespace SA