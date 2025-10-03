#ifndef LIBCFG_CG_H
#define LIBCFG_CG_H

#include <chrono>  // for time_point, nanoseconds, steady_clock
#include <cstdint> // for uint64_t
#include <iosfwd>  // for ostream
#include <map>     // for map
#include <memory>  // for shared_ptr
#include <string>  // for string

namespace CFG {
    class CGNode;
    class CG;
    struct Func;
    using CGTimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
    struct CGConstructionInfo {
        std::string name;
        CGTimePoint startTime;
        CGTimePoint endTime;
        double timeCost;
        uint64_t numOfFunc;
    };
} // namespace CFG

class CFG::CGNode {
public:
    CGNode() = delete;
    CGNode(std::shared_ptr<CFG::Func> f);

public:
    std::shared_ptr<CFG::Func> f;
    std::map<uint64_t, std::shared_ptr<CGNode>> callees;
    std::map<uint64_t, std::shared_ptr<CGNode>> callers;
};

class CFG::CG {
public:
    void toDot(std::ostream &os);

public:
    std::map<uint64_t, std::shared_ptr<CGNode>> nodes;
};

#endif