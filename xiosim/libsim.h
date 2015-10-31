/*
 * Libsim external API.
 * Copyright, Svilen Kanev, 2011
*/

#ifndef __LIBSIM_H__
#define __LIBSIM_H__

#include "host.h"

class handshake_container_t;

namespace xiosim {
namespace libsim {

int init(int argc, char** argv);
void deinit();

void simulate_handshake(int coreID, handshake_container_t* handshake);
void simulate_warmup(int asid, md_addr_t addr, bool is_write);

void activate_core(int coreID);
void deactivate_core(int coreID);
bool is_core_active(int coreID);

}  // xiosim::libsim
}  // xiosim

#endif /*__LIBSIM_H__ */
