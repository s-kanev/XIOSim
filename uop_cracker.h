#ifndef __UOP_CRACKER_H__
#define __UOP_CRACKER_H__

#include "zesto-structs.h"
#include "decode.h"

namespace xiosim {
namespace x86 {

/* Takes a decoded Mop and fills in the uop flow. */
void crack(struct Mop_t * Mop);

/* Deallocates the uops for a Mop */
void clear_uop_array(struct Mop_t * p);

}
}

#endif /* __UOP_CRACKER_H__ */
