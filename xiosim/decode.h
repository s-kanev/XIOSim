#ifndef __DECODE_H__
#define __DECODE_H__

#include <string>

extern "C" {
#include "xed-interface.h"
}

struct Mop_t;
struct uop_t;

namespace xiosim {
namespace x86 {

constexpr size_t MAX_ILEN = 15;
/* bytes, though we might be a bit inaccurate with anything > 16 */
constexpr size_t MAX_MEMOP_SIZE = 64;

void init_decoder();
void decode(struct Mop_t * Mop);
void decode_flags(struct Mop_t * Mop);

bool is_trap(const struct Mop_t * Mop);
bool is_ctrl(const struct Mop_t * Mop);
bool is_load(const struct Mop_t * Mop);
bool is_store(const struct Mop_t * Mop);
bool is_nop(const struct Mop_t * Mop);
/* Does Mop execute in FP-like cluster -- x87, SSE, AVX */
bool is_fp(const struct Mop_t * Mop);

inline const char * print_uop(const struct uop_t * uop) { return "NYI"; }

std::string print_Mop(const struct Mop_t * Mop);

xed_iclass_enum_t get_iclass(const struct Mop_t* Mop);

}  // xiosim::x86
}  // xiosim

#endif /* __DECODE_H__ */
