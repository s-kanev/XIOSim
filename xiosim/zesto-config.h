/* Read Zesto configuration file and store settings in the knobs structure.
 *
 * Cache, TLB, and prefetcher configurations are identical from an attributes
 * perspective. However, they differ in their default values. Therefore, each
 * such structure has its own specification.
 *
 * Author: Sam Xi
 */

#ifndef __ZESTO_CONFIG_H__
#define __ZESTO_CONFIG_H__

#include <string>

#include "knobs.h"

/* Entry point for parsing configuration file. Options are stored in the knobs structs. */
void read_config_file(std::string cfg_file, core_knobs_t* knobs, uncore_knobs_t* uncore_knobs,
                      system_knobs_t* system_knobs);

/* Print the parsed configuration for to file descriptor @fd. */
void print_config(FILE* fd);

/* Free all memory associated with the configs. */
void free_config();

#endif /* __ZESTO_CONFIG_H__ */
