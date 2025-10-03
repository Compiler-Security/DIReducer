#include "CFG/AsmBlock.h"
#include "CFG/Edge.h" // for Edge, EDGE_TYPE

namespace CFG {

    AsmBlock::AsmBlock() :
        sn(0), startAddr(0), type(CFG::BLOCK_TYPE::COMMON_BLOCK) {
    }

    std::vector<std::shared_ptr<CFG::AsmBlock>> AsmBlock::getAllPreds() const {
        std::vector<std::shared_ptr<AsmBlock>> res;
        for(auto it : this->backward_edges) {
            res.push_back(it->from);
        }

        return res;
    }

    std::vector<std::shared_ptr<CFG::AsmBlock>> AsmBlock::getAllSuccs() const {
        std::vector<std::shared_ptr<AsmBlock>> res;
        for(auto it : this->forward_edges) {
            res.push_back(it->to);
        }

        return res;
    }

    std::vector<std::shared_ptr<CFG::AsmBlock>> AsmBlock::getPredsExceptCall() const {
        std::vector<std::shared_ptr<AsmBlock>> res;
        for(auto it : this->backward_edges) {
            if(it->type != EDGE_TYPE::CALL && it->type != EDGE_TYPE::RETURN)
                res.push_back(it->from);
        }

        return res;
    }

    std::vector<std::shared_ptr<CFG::AsmBlock>> AsmBlock::getSuccsExceptCall() const {
        std::vector<std::shared_ptr<AsmBlock>> res;
        for(auto it : this->forward_edges) {
            if(it->type != EDGE_TYPE::CALL && it->type != EDGE_TYPE::RETURN)
                res.push_back(it->to);
        }

        return res;
    }

    /* EXIT_BLOCK, type=ENTRY, addr=0 */
    std::shared_ptr<AsmBlock> AsmBlock::start_block(uint32_t sn) {
        auto s       = std::make_shared<AsmBlock>();
        s->type      = CFG::BLOCK_TYPE::ENTRY_BLOCK;
        s->sn        = sn;
        s->startAddr = 0;
        return s;
    }

    /* EXIT_BLOCK, type=EXIT, addr=UINT64_MAX */
    std::shared_ptr<AsmBlock> AsmBlock::exit_block(uint32_t sn) {
        auto e       = std::make_shared<AsmBlock>();
        e->type      = CFG::BLOCK_TYPE::EXIT_BLOCK;
        e->sn        = sn;
        e->startAddr = UINT64_MAX;
        return e;
    }

    /* COMMON_BLOCK, type=COMMON, addr=addr */
    std::shared_ptr<AsmBlock> AsmBlock::common_block(uint32_t sn, uint64_t addr) {
        auto c       = std::make_shared<AsmBlock>();
        c->sn        = sn;
        c->startAddr = addr;
        return c;
    }

} // namespace CFG