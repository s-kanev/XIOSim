#ifndef __DECODE_H__
#define __DECODE_H__

#include <string>

#include "zesto-structs.h"

extern "C" {
#include "xed-interface.h"
}

namespace xiosim {
namespace x86 {

void init_decoder();
void decode(struct Mop_t * Mop);
void decode_flags(struct Mop_t * Mop);

bool is_trap(const struct Mop_t * Mop);
bool is_ctrl(const struct Mop_t * Mop);
bool is_load(const struct Mop_t * Mop);
bool is_store(const struct Mop_t * Mop);
bool is_nop(const struct Mop_t * Mop);
bool is_fence(const struct Mop_t * Mop);
/* Does Mop execute in FP-like cluster -- x87, SSE, AVX */
bool is_fp(const struct Mop_t * Mop);

inline const char * print_uop(const struct uop_t * uop) { return "NYI"; }

std::string print_Mop(const struct Mop_t * Mop);

inline xed_iclass_enum_t xed_iclass(const struct Mop_t * Mop)
{
    return xed_decoded_inst_get_iclass(&Mop->decode.inst);
}

}
}

#endif /* __DECODE_H__ */
