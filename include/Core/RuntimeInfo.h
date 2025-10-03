#ifndef TI_RUNTIMEINFO_H
#define TI_RUNTIMEINFO_H
#include <cstdint>
#include <stdint.h> // for uint32_t
#include <chrono>   // for time_point, nanoseconds, steady_clock

namespace SA {
    using TITimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
    struct RuntimeInfo {
        TITimePoint startTime;
        TITimePoint endTime;
        double timeCost;
        uint32_t numOfInsn;
        uint32_t numOfBasicBlock;
        uint32_t numOfEdges;
        uint32_t totalOP;
        uint32_t inferredOP;
        uint32_t irrelevantOP;
        uint32_t totalVarNum;
        uint32_t usedVarNum;
        RuntimeInfo();
        void append(const RuntimeInfo &other);
    };

} // namespace SA
#endif