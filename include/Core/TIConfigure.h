#ifndef TI_CONFIGURE_H
#define TI_CONFIGURE_H

namespace SA {
    struct TIConfigure {
        TIConfigure();
        bool enableDynLibFuncAnalysis;
        bool enableGlobalVariablAnalysis;
        bool enableInterProceduralAnalysis;
        bool enableMemOPCalculatedAnalysis;
        bool enableSavingRegCompensation;
        bool enableUsingUnknownVar;
    };

} // namespace SA
#endif