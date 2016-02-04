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

/* Max number of uops per Mop */
const size_t MAX_NUM_UOPS = 64;
const size_t UOP_SEQ_SHIFT = 6; // log2(MAX_NUM_UOPS)

/* Max number of input registers per uop. */
const size_t MAX_IDEPS = 3;
/* Max numbder of output registers per uop (1+flags). */
const size_t MAX_ODEPS = 2;

/* Flags for allowing different types of uop fusion. */
struct fusion_flags_t {
    bool LOAD_OP:1;
    bool STA_STD:1;
    bool LOAD_OP_ST:1; /* for atomic Mop execution */
    bool FP_LOAD_OP:1; /* same as load op, but for fp ops */

    bool matches(fusion_flags_t& rhs) {
        return (LOAD_OP && rhs.LOAD_OP) || (STA_STD && rhs.STA_STD) ||
               (LOAD_OP_ST && rhs.LOAD_OP_ST) || (FP_LOAD_OP && rhs.FP_LOAD_OP);
    }
};

}  // xiosim::x86
}  // xiosim

#endif /* __UOP_CRACKER_H__ */
