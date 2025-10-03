#ifndef LIBCFG_ASM_CFG_H
#define LIBCFG_ASM_CFG_H

#include <cstdint>
#include <chrono>
#include <iostream>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "AsmBlock.h"
#include "Edge.h"

namespace CFG {
    struct AsmCFG;
    using CFGTimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
    struct CFGConstructionInfo {
        std::string name;
        CFGTimePoint startTime;
        CFGTimePoint endTime;
        double timeCost;
        uint64_t size;
        uint64_t numOfInsn;
        uint64_t numOfFunc;
        uint64_t numOfEdge;
        uint64_t numOfBasciBlock;
    };
} // namespace CFG

struct CFG::AsmCFG {
    std::string name;

    uint64_t low_pc;

    uint64_t high_pc;

    std::unordered_map<uint64_t, std::shared_ptr<AsmBlock>> blocks;

    std::unordered_map<uint64_t, uint32_t> addrSnMap;

    std::vector<std::shared_ptr<Edge>> edges;

private:
    uint32_t entry_sn;
    uint32_t exit_sn;
    std::shared_ptr<char[]> elfBuffer;

    /// @brief 用于指示VEX是否初始化
    static bool isVEXinitialized;

private:
    /// @brief 获取间接跳转指令可能的跳转表的起始地址
    /// @param instructions 当前指令
    /// @param curIndex 当前指令索引
    std::optional<uint64_t> lookBackForJumpTableAddr(const AsmStatement *instructions, uint32_t curIndex);

    /// @brief 获取间接跳转指令可能的跳转地址
    /// @param jumpTableAddr 跳转表的起始地址
    /// @param allInsnAddr 本函数内所有指令地址
    std::set<uint64_t> getJumpAddr(uint64_t jumpTableAddr, const std::set<uint64_t> &allInsnAddr);

    /// @brief 获取基本块的首地址
    /// @param instructions 指令数组指针
    /// @param num_of_insn 指令数量
    /// @param allInsnAddr 本函数内所有指令地址
    std::set<uint64_t> getBasicBlockHeadAddr(const AsmStatement *instructions, uint32_t num_of_insn, const std::set<uint64_t> &allInsnAddr);

    /// @brief 初始化基本块中的语句
    /// @param instructions 指令数组指针
    /// @param num_of_insn 指令数量
    void initStatementsOfBasicBlock(const AsmStatement *instructions, uint32_t num_of_insn);

    /// @brief 初始化基本块中的边
    /// @param allInsnAddr 本函数内所有指令地址
    void initEdgesOfBasicBlock(const std::set<uint64_t> &allInsnAddr);

    /// @brief 初始化基本块的VEX IR
    /// @brief 调用时会设置AsmCFG::isVEXinitialized为true
    void initIRSBofBasicBlock();

    /// @brief 获取函数出口基本块,若尚未设置则添加
    std::shared_ptr<CFG::AsmBlock> getOrSetExitBlock();

public:
    AsmCFG() = default;
    AsmCFG(std::string &name, const AsmStatement *instructions, uint32_t num_of_insn, const std::shared_ptr<char[]> &elfBuffer);

    /// @brief 输出CFG对应的dot文件,默认输出到标准输出
    void toDot(std::ostream &os);

    /// @brief 获取函数入口基本块
    std::shared_ptr<CFG::AsmBlock> getEntryBlock();

    /// @brief 获取函数出口基本块
    std::optional<std::shared_ptr<CFG::AsmBlock>> getExitBlock();
};

#endif