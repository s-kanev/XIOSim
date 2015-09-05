#ifndef __UOP_CRACKER_H__
#define __UOP_CRACKER_H__

struct Mop_t;
struct uop_t;

namespace xiosim {
namespace x86 {

/* Takes a decoded Mop and fills in the uop flow. */
void crack(struct Mop_t * Mop);

/* Allocate a (properly aligned) array of uops. */
struct uop_t* get_uop_array(const size_t num_uops);

/* Deallocates the uops for a Mop */
void return_uop_array(struct uop_t* p, const size_t num_uops);

const size_t MAX_NUM_UOPS = 64;
const size_t UOP_SEQ_SHIFT = 6;

}
}

#endif /* __UOP_CRACKER_H__ */
