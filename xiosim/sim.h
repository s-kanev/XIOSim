#ifndef SIM_H
#define SIM_H

namespace xiosim {

namespace stats {
class StatsDatabase;  // fwd
}

namespace libsim {

/* register simulation statistics */
void sim_reg_stats(xiosim::stats::StatsDatabase* sdb);
void compute_rtp_power(void);

}  // xiosim::libsim
}  // xiosim

#endif /* SIM_H */
