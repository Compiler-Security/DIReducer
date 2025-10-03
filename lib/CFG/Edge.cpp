#include "CFG/Edge.h"

namespace CFG {
    Edge::Edge(std::shared_ptr<AsmBlock> &from, std::shared_ptr<AsmBlock> &to, CFG::EDGE_TYPE type) :
        from(from), to(to), type(type) {
    }

} // namespace CFG