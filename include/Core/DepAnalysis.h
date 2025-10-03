#ifndef TI_DEP_ANALYSIS_H
#define TI_DEP_ANALYSIS_H

#include <unordered_map>
#include "TIOperand.h"

namespace SA {
    struct DepNode {
        uint32_t varId;
        uint32_t offset;
        DepNode() :
            varId(0), offset(0) {
        }
        DepNode(uint32_t varId, uint32_t offset) :
            varId(varId), offset(offset) {
        }
        bool operator==(const DepNode &other) const {
            return varId == other.varId && offset == other.offset;
        }
        bool operator<(const DepNode &other) const {
            return varId < other.varId || (varId == other.varId && offset < other.offset);
        }
    };
    struct DepNodeHash {
        uint64_t operator()(const DepNode &node) const {
            return (static_cast<uint64_t>(node.varId) << 32) + node.offset;
        }
    };

    struct DepContext : public std::unordered_map<TIOperandRef, std::set<DepNode>> {};

    struct DepGraph : public std::unordered_map<DepNode, std::set<DepNode>, DepNodeHash> {
        void solve();
    };

    bool unionDepContext(DepContext &dst, const DepContext &src);

    void removeRegFromDepContext(DepContext &ctx, x86_reg reg);
} // namespace SA


#endif