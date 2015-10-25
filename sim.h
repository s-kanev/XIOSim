#ifndef SIM_H
#define SIM_H

#include "stats.h"
#include "zesto-structs.h"

extern struct core_knobs_t knobs;

/* spin on assertion failure so we can attach a debbuger */
extern bool assert_spin;

namespace xiosim {

const int MAX_CORES = 16;
const int INVALID_CORE = -1;

}  // xiosim

/* number of cores */
extern int num_cores;
extern struct core_t** cores;

namespace xiosim {
namespace libsim {

/* register simulation statistics */
void sim_reg_stats(xiosim::stats::StatsDatabase* sdb);
void compute_rtp_power(void);

}  // xiosim::libsim
}  // xiosim

#endif /* SIM_H */
