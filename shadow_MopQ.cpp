#include "zesto-structs.h"
#include "zesto-oracle.h"
#include "zesto-fetch.h"
#include "shadow_MopQ.h"

/* Get the shadow MopQ entry corresponding to Mop in the oracle MopQ.
 * The oracle calls this with a fresh new Mop that it needs to decode and process.
 */
handshake_container_t const& shadow_MopQ_t::get_shadow_Mop(const struct Mop_t* Mop) {
    struct core_t* core = Mop->core;
    core_oracle_t* oracle = core->oracle;

    int Mop_ind = oracle->get_index(Mop);
    int from_head = (Mop_ind - oracle->MopQ_head) & (oracle->MopQ_size - 1);

    /* Non-spec Mops are always in sync with the MopQ. So, we just need the same position
     * in this buffer. */
    if (!Mop->oracle.spec_mode) {
        handshake_container_t& res = buffer_.get_item(from_head)->Mop;
        zesto_assert(res.flags.valid, nullptr);
        return res;
    } else {
        /* Speculative Mops in the MopQ are always on the tail end (newest) -- when we recover
         * from speculation, we blow them away. */

        /* So, to get to a speculative entry, we first find the youngest non-spec Mop
         * in the MopQ (the one that triggered speculation. */
        zesto_assert(oracle->MopQ_num - oracle->MopQ_spec_num > 0, nullptr);
        int last_non_spec_Mop = moddec(oracle->MopQ_non_spec_tail, oracle->MopQ_size);
        int non_spec_from_head = (last_non_spec_Mop - oracle->MopQ_head) & (oracle->MopQ_size - 1);
        /* Because non-spec Mops are always in sync, it's business as usual. */
        auto shadow_entry = buffer_.get_item(non_spec_from_head);

        /* We need to check diff # Mops deep in the speculation queue. */
        int diff = from_head - non_spec_from_head - 1;
        zesto_assert(diff >= 0, nullptr);

        /* Feeder didn't give us enough speculative handshakes. And we don't have fake
         * NOPs manufactured.
         * The only time this can happen is when we encounter new (unseen before) speculation
         * on a nuke recovery path. So, just get new fake NOPs. */
        while (diff >= (int)shadow_entry->speculated.size()) {
            zesto_assert(oracle->on_nuke_recovery_path(), nullptr);
            shadow_entry->speculated.push_back(oracle->get_fake_spec_handshake());
            size_++;
        }

        /* We have a spec entry in place. Just return it. */
        handshake_container_t& res = shadow_entry->speculated[diff];
        zesto_assert(res.flags.valid, nullptr);
        return res;
    }
}
