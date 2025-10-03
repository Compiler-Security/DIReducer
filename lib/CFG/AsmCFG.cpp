#include <stdint.h>            // for uint64_t, uint32_t
#include <string.h>            // for NULL, memset
#include <iostream>            // for basic_ostream, operator<<, endl
#include <memory>              // for shared_ptr, __shared_ptr_access
#include <optional>            // for optional
#include <set>                 // for set, _Rb_tree_const_iterator
#include <string>              // for char_traits, operator<<, string
#include <unordered_map>       // for unordered_map, operator==, _Node_...
#include <utility>             // for pair, make_pair
#include <vector>              // for vector
#include "CFG/AsmBlock.h"      // for AsmBlock, BLOCK_TYPE
#include "CFG/AsmCFG.h"        // for AsmCFG
#include "CFG/AsmStatement.h"  // for AsmStatement
#include "CFG/Edge.h"          // for Edge, EDGE_TYPE
#include "CFG/Utils.h"         // for getUniqueID, hexStringToUInt, isH...
#include "Fig/assert.hpp"      // for FIG_ASSERT
#include "Fig/panic.hpp"       // for FIG_PANIC
#include "capstone/capstone.h" // for cs_insn
#include "capstone/x86.h"      // for x86_insn, x86_op_type, x86_reg

namespace CFG {

    bool AsmCFG::isVEXinitialized = false;

    AsmCFG::AsmCFG(std::string &name, const AsmStatement *instructions, uint32_t num_of_insn, const std::shared_ptr<char[]> &elfBuffer) {
        FIG_ASSERT(num_of_insn != 0, "The number of instructions is zero!");

        this->name      = name;
        this->elfBuffer = elfBuffer;
        this->low_pc    = instructions[0].address;
        this->high_pc   = instructions[num_of_insn - 1].address;

        this->entry_sn = getUniqueID();
        this->blocks.insert(std::make_pair(this->entry_sn, CFG::AsmBlock::start_block(this->entry_sn)));
        this->exit_sn = getUniqueID();
        this->blocks.insert(std::make_pair(this->exit_sn, CFG::AsmBlock::exit_block(this->exit_sn)));
        std::set<uint64_t> allInsnAddr = {};
        for(uint32_t i = 0; i < num_of_insn; i++) {
            allInsnAddr.insert(instructions[i].address);
        }
        std::set<uint64_t> bb_head_addr_set = AsmCFG::getBasicBlockHeadAddr(instructions, num_of_insn, allInsnAddr);
        for(uint64_t startAddr : bb_head_addr_set) {
            auto blockSn = getUniqueID();
            this->addrSnMap.insert(std::make_pair(startAddr, blockSn));
            this->blocks.insert(std::make_pair(blockSn, CFG::AsmBlock::common_block(blockSn, startAddr)));
        }

        this->initStatementsOfBasicBlock(instructions, num_of_insn);
        this->initEdgesOfBasicBlock(allInsnAddr);

        // no need to init VEX IR
    }

    std::set<uint64_t> AsmCFG::getBasicBlockHeadAddr(const AsmStatement *instructions, uint32_t num_of_insn, const std::set<uint64_t> &allInsnAddr) {
        if(num_of_insn == 0)
            return {};

        uint64_t bb_low_pc  = instructions[0].address;
        uint64_t bb_high_pc = instructions[num_of_insn - 1].address;
        std::set<uint64_t> bb_head_addr_set{instructions[0].address};
        for(uint32_t i = 0; i < num_of_insn; i++) {
            auto &insn = instructions[i];
            switch(insn.id) {
            /* JUMP Instructments */
            case X86_INS_LJMP:
            case X86_INS_JAE:
            case X86_INS_JA:
            case X86_INS_JBE:
            case X86_INS_JB:
            case X86_INS_JCXZ:
            case X86_INS_JECXZ:
            case X86_INS_JE:
            case X86_INS_JGE:
            case X86_INS_JG:
            case X86_INS_JLE:
            case X86_INS_JL:
            case X86_INS_JNE:
            case X86_INS_JNO:
            case X86_INS_JNP:
            case X86_INS_JNS:
            case X86_INS_JO:
            case X86_INS_JP:
            case X86_INS_JRCXZ:
            case X86_INS_JS:
            case X86_INS_JMP: {
                const auto &detail = insn.detail->x86;
                const auto &op0    = detail.operands[0];
                if(isHexadecimal(insn.op_str)) {
                    uint64_t addr = hexStringToUInt(insn.op_str);
                    if(addr >= bb_low_pc && addr <= bb_high_pc)
                        bb_head_addr_set.insert(addr);
                } else if(insn.id == X86_INS_JMP && op0.type == X86_OP_REG && op0.reg == X86_REG_RAX) {
                    auto jumpTableAddrOpt = this->lookBackForJumpTableAddr(instructions, i);
                    if(jumpTableAddrOpt.has_value()) {
                        for(uint64_t targetAddr : this->getJumpAddr(jumpTableAddrOpt.value(), allInsnAddr)) {
                            bb_head_addr_set.insert(targetAddr);
                        }
                    }
                }
                if(i < num_of_insn - 1)
                    bb_head_addr_set.insert(instructions[i + 1].address);
                break;
            }
            /* RETURN Instructments */
            case X86_INS_RET: {
                if(i < num_of_insn - 1)
                    bb_head_addr_set.insert(instructions[i + 1].address);
                break;
            }
            /* CALL Instructments */
            case X86_INS_CALL: {
                if(i < num_of_insn - 1)
                    bb_head_addr_set.insert(instructions[i + 1].address);
                break;
            }
            default: break;
            }
        }

        return bb_head_addr_set;
    }

    std::optional<uint64_t> AsmCFG::lookBackForJumpTableAddr(const AsmStatement *instructions, uint32_t curIndex) {
        if(curIndex < 2)
            return {};
        if(!(instructions[curIndex - 1].id == X86_INS_ADD && instructions[curIndex - 2].id == X86_INS_LEA))
            return {};
        const auto &addInsn   = instructions[curIndex - 1];
        const auto &leaInsn   = instructions[curIndex - 2];
        const auto &leaDetail = leaInsn.detail->x86;
        const auto &leaOP1    = leaDetail.operands[1];
        if(leaOP1.type != X86_OP_MEM)
            return {};
        if(leaOP1.mem.base != X86_REG_RIP || leaOP1.mem.disp == 0)
            return {};

        return addInsn.address + leaOP1.mem.disp;
    }

    std::set<uint64_t> AsmCFG::getJumpAddr(uint64_t jumpTableAddr, const std::set<uint64_t> &allInsnAddr) {
        char *rawBuffer        = this->elfBuffer.get();
        int *jumpTablePtr      = reinterpret_cast<int *>(rawBuffer + jumpTableAddr);
        std::set<uint64_t> res = {};
        uint32_t index         = 0;
        while(true) {
            uint64_t targetAddr = jumpTableAddr + jumpTablePtr[index];
            if(!(targetAddr > this->low_pc && targetAddr < this->high_pc))
                break;
            if(!allInsnAddr.contains(targetAddr))
                break;

            res.insert(targetAddr);
            index++;
        }

        return res;
    }

    void AsmCFG::initStatementsOfBasicBlock(const AsmStatement *instructions, uint32_t num_of_insn) {
        std::shared_ptr<CFG::AsmBlock> cur_block = this->getEntryBlock();
        for(uint32_t i = 0; i < num_of_insn; i++) {
            auto &insn = instructions[i];
            if(this->addrSnMap.find(insn.address) != this->addrSnMap.end()) {
                // get next block
                auto next_block = this->blocks[this->addrSnMap[insn.address]];

                // assign startAddr
                if(next_block->type == BLOCK_TYPE::COMMON_BLOCK)
                    next_block->startAddr = insn.address;

                // assign endAddr
                if(cur_block->type == BLOCK_TYPE::COMMON_BLOCK)
                    cur_block->endAddr = insn.address;

                // assign forward/backward block
                cur_block->next_block      = next_block;
                next_block->previous_block = cur_block;

                // reset cur block
                cur_block = next_block;
            }
            cur_block->statements.push_back(insn);
        }
        // set last block's endAddr
        const auto &last_insn = instructions[num_of_insn - 1];
        if(cur_block->type == BLOCK_TYPE::COMMON_BLOCK)
            cur_block->endAddr = last_insn.address + last_insn.size;
    }

    void AsmCFG::initEdgesOfBasicBlock(const std::set<uint64_t> &allInsnAddr) {
        for(auto &it : this->blocks) {
            auto &block = it.second;
            switch(block->type) {
            case CFG::BLOCK_TYPE::EXIT_BLOCK: {
                break;
            }
            case CFG::BLOCK_TYPE::ENTRY_BLOCK: {
                if(block->next_block.has_value()) {
                    std::shared_ptr<Edge> e = std::make_shared<Edge>(block, block->next_block.value(), EDGE_TYPE::DEFAULT_EDGE);
                    block->forward_edges.push_back(e);
                    block->next_block.value()->backward_edges.push_back(e);
                    this->edges.push_back(e);
                }
                break;
            }
            case CFG::BLOCK_TYPE::COMMON_BLOCK: {
                const auto &last_insn = block->statements.back();
                switch(last_insn.id) {
                case X86_INS_LJMP:
                case X86_INS_JMP: {
                    const auto &detail = last_insn.detail->x86;
                    const auto &op0    = detail.operands[0];
                    if(isHexadecimal(last_insn.op_str)) {
                        uint64_t addr = hexStringToUInt(last_insn.op_str);
                        if(this->addrSnMap.find(addr) != this->addrSnMap.end()) {
                            std::shared_ptr<Edge> e = std::make_shared<Edge>(block, this->blocks[this->addrSnMap[addr]], EDGE_TYPE::DEFAULT_EDGE);
                            block->forward_edges.push_back(e);
                            this->blocks[this->addrSnMap[addr]]->backward_edges.push_back(e);
                            this->edges.push_back(e);
                        }
                    } else if(last_insn.id == X86_INS_JMP && op0.type == X86_OP_REG && op0.reg == X86_REG_RAX) {
                        auto jumpTableAddrOpt = this->lookBackForJumpTableAddr(block->statements.data(), block->statements.size() - 1);
                        if(jumpTableAddrOpt.has_value()) {
                            for(uint64_t targetAddr : this->getJumpAddr(jumpTableAddrOpt.value(), allInsnAddr)) {
                                if(!this->addrSnMap.contains(targetAddr))
                                    continue;
                                std::shared_ptr<Edge> e = std::make_shared<Edge>(block, this->blocks[this->addrSnMap[targetAddr]], EDGE_TYPE::DEFAULT_EDGE);
                                block->forward_edges.push_back(e);
                                this->blocks[this->addrSnMap[targetAddr]]->backward_edges.push_back(e);
                                this->edges.push_back(e);
                            }
                        }
                    }
                    break;
                }
                case X86_INS_JAE:
                case X86_INS_JA:
                case X86_INS_JBE:
                case X86_INS_JB:
                case X86_INS_JCXZ:
                case X86_INS_JECXZ:
                case X86_INS_JE:
                case X86_INS_JGE:
                case X86_INS_JG:
                case X86_INS_JLE:
                case X86_INS_JL:
                case X86_INS_JNE:
                case X86_INS_JNO:
                case X86_INS_JNP:
                case X86_INS_JNS:
                case X86_INS_JO:
                case X86_INS_JP:
                case X86_INS_JRCXZ:
                case X86_INS_JS: {
                    if(block->next_block.has_value()) {
                        std::shared_ptr<Edge> e = std::make_shared<Edge>(block, block->next_block.value(), EDGE_TYPE::BRANCH_FALSE);
                        block->forward_edges.push_back(e);
                        block->next_block.value()->backward_edges.push_back(e);
                        this->edges.push_back(e);
                    }
                    if(isHexadecimal(last_insn.op_str)) {
                        uint64_t addr = hexStringToUInt(last_insn.op_str);
                        if(this->addrSnMap.find(addr) != this->addrSnMap.end()) {
                            std::shared_ptr<Edge> e = std::make_shared<Edge>(block, this->blocks[this->addrSnMap[addr]], EDGE_TYPE::BRANCH_TRUE);
                            block->forward_edges.push_back(e);
                            this->blocks[this->addrSnMap[addr]]->backward_edges.push_back(e);
                            this->edges.push_back(e);
                        }
                    }
                    break;
                }
                case X86_INS_RET: {
                    auto exit_block         = this->getOrSetExitBlock();
                    std::shared_ptr<Edge> e = std::make_shared<Edge>(block, exit_block, EDGE_TYPE::DEFAULT_EDGE);
                    block->forward_edges.push_back(e);
                    exit_block->backward_edges.push_back(e);
                    this->edges.push_back(e);
                    break;
                }
                case X86_INS_CALL:
                default: {
                    if(block->next_block.has_value()) {
                        std::shared_ptr<Edge> e = std::make_shared<Edge>(block, block->next_block.value(), EDGE_TYPE::CALL_RETURN);
                        block->forward_edges.push_back(e);
                        block->next_block.value()->backward_edges.push_back(e);
                        this->edges.push_back(e);
                    }
                    break;
                }
                }
                break;
            }
            default: break;
            }
        }
    }

    void AsmCFG::toDot(std::ostream &os) {
        os << "digraph \"CFG for '" << this->name << "' function\" {" << std::endl;
        os << "label=\"CFG for '" << this->name << "' function\";" << std::endl;
        for(auto &it : this->blocks) {
            auto &block = it.second;
            os << "Block" << block->sn;
            os << "[shape=record, color=\"#3d50c3ff\", style=filled, fillcolor=\"#b9d0f970\",";
            if(block->type == CFG::BLOCK_TYPE::ENTRY_BLOCK)
                os << "label=\"{" << this->name << "::ENTRY"
                   << "\\l ";
            else if(block->type == CFG::BLOCK_TYPE::EXIT_BLOCK)
                os << "label=\"{" << this->name << "::EXIT"
                   << "\\l ";
            else
                os << "label=\"{B" << block->sn << "\\l ";

            for(auto &insn : block->statements) {
                os << "0x" << dec_to_hex(insn.address) << ":   " << insn.mnemonic << "  " << insn.op_str << "\\l ";
            }
            os << "}\""
               << "];" << std::endl;
            for(auto &e : block->forward_edges) {
                if(e->type == EDGE_TYPE::CALL || e->type == EDGE_TYPE::RETURN)
                    continue;
                os << "Block" << e->from->sn << " -> "
                   << "Block" << e->to->sn << ";\n";
            }
        }
        os << "}" << std::endl;
    }

    std::shared_ptr<CFG::AsmBlock> AsmCFG::getEntryBlock() {
        return this->blocks[this->entry_sn];
    }

    std::optional<std::shared_ptr<CFG::AsmBlock>> AsmCFG::getExitBlock() {
        if(this->exit_sn == 0)
            return {};
        else
            return this->blocks[this->exit_sn];
    }

    std::shared_ptr<CFG::AsmBlock> AsmCFG::getOrSetExitBlock() {
        if(this->exit_sn == 0) {
            this->exit_sn = getUniqueID();
            this->blocks.insert(std::make_pair(this->exit_sn, CFG::AsmBlock::exit_block(this->exit_sn)));
        }
        return this->blocks[this->exit_sn];
    }
} // namespace CFG