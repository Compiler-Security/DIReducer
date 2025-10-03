#include "CFG/CG.h"
#include <ostream>    // for basic_ostream, operator<<, endl, basic_ios
#include "CFG/Func.h" // for Func

namespace CFG {
    CGNode::CGNode(std::shared_ptr<CFG::Func> f) :
        f(f) {
    }

    void CG::toDot(std::ostream &os) {
        os << "digraph \"Call Graph \"{" << std::endl;
        os << "label=\"Call Graph\";" << std::endl;
        for(const auto &[addr, node] : this->nodes) {
            os << "Block" << addr;
            os << "[shape=record, color=\"#3d50c3ff\", style=filled, fillcolor=\"#b9d0f970\",";
            os << "label=\"{" << node->f->dwarfName << "\\l ";
            os << "}\""
               << "];" << std::endl;
            for(const auto &[calleeName, callee] : node->callees) {
                os << "Block" << addr << " -> "
                   << "Block" << calleeName << ";\n";
            }
        }
        os << "}" << std::endl;
    }
} // namespace CFG