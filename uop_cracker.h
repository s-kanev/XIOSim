#ifndef __UOP_CRACKER_H__
#define __UOP_CRACKER_H__

#include "zesto-structs.h"
#include "decode.h"

namespace xiosim {
namespace x86 {

/* Takes a decoded Mop and fills in the uop flow. */
void crack(struct Mop_t * Mop);

/* Allocate a (properly aligned) array of uops.
 * They can be safely "delete"-d without a Mop structure. */
struct uop_t * get_uop_array(const size_t size);

/* Deallocates the uops for a Mop */
void clear_uop_array(struct Mop_t * p);

const size_t MAX_NUM_UOPS = 64;
const size_t UOP_SEQ_SHIFT = 6;

}
}

#endif /* __UOP_CRACKER_H__ */
