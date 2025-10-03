#include "Core/RuntimeInfo.h"

namespace SA {
    RuntimeInfo::RuntimeInfo() {
        this->timeCost        = 0;
        this->numOfInsn       = 0;
        this->numOfBasicBlock = 0;
        this->numOfEdges      = 0;
        this->totalOP         = 0;
        this->inferredOP      = 0;
        this->irrelevantOP    = 0;
        this->totalVarNum     = 0;
        this->usedVarNum      = 0;
    }

    void RuntimeInfo::append(const RuntimeInfo &other) {
        this->timeCost += other.timeCost;
        this->numOfInsn += other.numOfInsn;
        this->numOfBasicBlock += other.numOfBasicBlock;
        this->numOfEdges += other.numOfEdges;
        this->totalOP += other.totalOP;
        this->inferredOP += other.inferredOP;
        this->irrelevantOP += other.irrelevantOP;
        this->totalVarNum += other.totalVarNum;
        this->usedVarNum += other.usedVarNum;
    }
} // namespace SA