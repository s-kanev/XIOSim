#ifndef __SHADOW_MOPQ__
#define __SHADOW_MOPQ__

#include <vector>

#include "pintool/buffer.h"
#include "pintool/handshake_container.h"
#include "zesto-oracle.h"

class shadow_Mop_t {
  public:
    handshake_container_t Mop;
    std::vector<handshake_container_t> speculated;

    void Invalidate() {
        speculated.clear();
        Mop.Invalidate();
    }
};

/* The shadow MopQ is a circular buffer holding all handshakes that we've received from the
 * instruction feeder. It is closely synced with the MopQ in the oracle -- in fact,
 * the oracle MopQ is always filled with entries from the shadow MopQ.
 *
 * We need to buffer all handshakes because of load-store order violations ('nukes'),
 * which might need to roll back oracle state hundreds of cycles (and instructions)
 * after we've consumed a Mop. When we detect a nuke, we clear the oracle MopQ,
 * and start the recovery with handshakes from the shadow MopQ, until we need new
 * ones from the feeder. Check libsim::simulate_handshake() for that logic.
 *
 * As opposed to the MopQ, which has speculative Mops right after non-spec ones,
 * here they hang in a vector on the side of the Mop that triggered the speculation.
 * This is mostly to make it easier to keep them around on speculation recovery paths.
 * On (non-nuke) spec recovery, we don't blow away shadow Mops (as we do with Mops in the
 * MopQ) -- we always keep them around until commit, in case a nuke happens later and we
 * need to go down the recovery path and happen to speculate on it.
 *
 * Invariant: non-speculative Mops are in sync between the MopQ and the shadow_MopQ --
 * that is, speculation aside, MopQ[X] should always be filled from shadow_MopQ[X].
 */
class shadow_MopQ_t {
  public:
    shadow_MopQ_t(size_t buff_size)
        : buffer_(buff_size)
        , size_(0) {}

    /* Get the shadow MopQ entry corresponding to Mop in the oracle MopQ. */
    handshake_container_t const& get_shadow_Mop(const struct Mop_t* Mop);

    /* Push the newest element to the queue. */
    void push_handshake(const handshake_container_t* handshake) {
        if (!handshake->flags.speculative) {
            handshake_container_t* shadow_handshake = &buffer_.get_buffer()->Mop;
            new (shadow_handshake) handshake_container_t(*handshake);
            buffer_.push_done();
        } else {
            buffer_.back()->speculated.push_back(*handshake);
        }
        size_++;
    }

    bool full(void) const { return buffer_.full(); }

    bool empty(void) const { return buffer_.empty(); }

    int size(void) const { return size_; }

    int non_spec_size(void) const { return buffer_.size(); }

    /* Pops the oldest non-speculative shadow_MopQ entry, as well as any speculative
     * ones it might have triggered. We don't need to keep them around any more. */
    void pop(void) {
        assert(!buffer_.front()->Mop.flags.speculative);
        size_--;
        size_ -= buffer_.front()->speculated.size();
        buffer_.pop();
        assert(size() >= 0);
    }

  private:
    Buffer<shadow_Mop_t> buffer_;

    int size_;
};

#endif /* __SHADOW_MOPQ__ */
