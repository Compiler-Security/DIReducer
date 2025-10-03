#ifndef LIBCFG_ASM_STATEMENT_H
#define LIBCFG_ASM_STATEMENT_H

#include "capstone/capstone.h"
#include <string>

namespace CFG {
    using AsmStatement = cs_insn;

    std::string insnToString(const AsmStatement &insn);
} // namespace CFG

#endif