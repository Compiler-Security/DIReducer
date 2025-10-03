#ifndef LIBCFG_EDGE_H
#define LIBCFG_EDGE_H

#include <memory> // for shared_ptr

namespace CFG {
    struct Edge;
    struct AsmBlock;
    enum class EDGE_TYPE {
        CALL,
        RETURN,
        CALL_RETURN,
        BRANCH_TRUE,
        BRANCH_FALSE,
        DEFAULT_EDGE,
    };

} // namespace CFG

struct CFG::Edge {
    std::shared_ptr<AsmBlock> from;
    std::shared_ptr<AsmBlock> to;
    CFG::EDGE_TYPE type;

    Edge(std::shared_ptr<AsmBlock> &from, std::shared_ptr<AsmBlock> &to, CFG::EDGE_TYPE type);
};

#endif