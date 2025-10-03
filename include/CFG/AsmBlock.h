#ifndef LIBCFG_ASM_BLOCK_H
#define LIBCFG_ASM_BLOCK_H

#include <cstdint>         // for uint32_t, uint64_t
#include <memory>          // for shared_ptr
#include <optional>        // for optional
#include <vector>          // for vector
#include "AsmStatement.h"  // for AsmStatement

namespace CFG {
    struct AsmBlock;
    struct Edge;

    enum class BLOCK_TYPE {
        ENTRY_BLOCK,
        EXIT_BLOCK,
        COMMON_BLOCK
    };
} // namespace CFG

struct CFG::AsmBlock {
public:
    AsmBlock();
    std::vector<std::shared_ptr<CFG::AsmBlock>> getAllPreds() const;
    std::vector<std::shared_ptr<CFG::AsmBlock>> getAllSuccs() const;
    std::vector<std::shared_ptr<CFG::AsmBlock>> getPredsExceptCall() const;
    std::vector<std::shared_ptr<CFG::AsmBlock>> getSuccsExceptCall() const;
    static std::shared_ptr<AsmBlock> start_block(uint32_t sn);
    static std::shared_ptr<AsmBlock> exit_block(uint32_t sn);
    static std::shared_ptr<AsmBlock> common_block(uint32_t sn, uint64_t addr);

public:
    uint32_t sn;
    uint64_t startAddr;
    uint64_t endAddr;
    CFG::BLOCK_TYPE type;

    std::vector<AsmStatement> statements;
    std::optional<std::shared_ptr<AsmBlock>> next_block;
    std::optional<std::shared_ptr<AsmBlock>> previous_block;
    std::vector<std::shared_ptr<Edge>> forward_edges;
    std::vector<std::shared_ptr<Edge>> backward_edges;
};

#endif