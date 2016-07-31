#ifndef _TCM_OPTS_H_
#define _TCM_OPTS_H_

/* Interfaces for each optimizations.
 *
 * Each optimization is implemented in its own namespace with the same interface:
 *
 *   - RegisterEmulation
 *   - GetBaselineReplacements
 *   - LocateMagicSequence (optional)
 *   - GetIdealReplacements (optional)
 *   - GetRealisticReplacements (optional)
 */

#include <vector>
#include "tcm_utils.h"

namespace SLLPop {
void RegisterEmulation(INS ins);
repl_vec_t GetIdealReplacements(const insn_vec_t& insns);
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns);
}

namespace SLLPush {
void RegisterEmulation(INS ins);
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns);
}

namespace LLHeadCacheLookup {
void RegisterEmulation(INS ins);
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns);
repl_vec_t GetRealisticReplacements(const insn_vec_t& insns);
insn_vec_t LocateMagicSequence(const INS& ins);
}

namespace LLHeadCacheUpdate {
void RegisterEmulation(INS ins);
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns);
insn_vec_t LocateMagicSequence(const INS& ins);
}

namespace SizeClassCacheLookup {
void RegisterEmulation(INS ins);
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns);
repl_vec_t GetRealisticReplacements(const insn_vec_t& insns);
insn_vec_t LocateMagicSequence(const INS& ins);
}

namespace SizeClassCacheUpdate {
void RegisterEmulation(INS ins);
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns);
insn_vec_t LocateMagicSequence(const INS& ins);
}

namespace Sampling {
void RegisterEmulation(INS ins);
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns);
insn_vec_t LocateMagicSequence(const INS& ins);
}

#endif
