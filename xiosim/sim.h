#ifndef SIM_H
#define SIM_H

#include "knobs.h"
#include "stats.h"

extern struct core_knobs_t core_knobs;
extern struct uncore_knobs_t uncore_knobs;
extern struct system_knobs_t system_knobs;

namespace xiosim {

const int MAX_CORES = 16;
const int INVALID_CORE = -1;

}  // xiosim

extern struct core_t** cores;

namespace xiosim {
namespace libsim {

/* register simulation statistics */
void sim_reg_stats(xiosim::stats::StatsDatabase* sdb);
void compute_rtp_power(void);

}  // xiosim::libsim
}  // xiosim

#endif /* SIM_H */
