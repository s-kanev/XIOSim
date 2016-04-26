extern time_t last_time;
#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>

/* Instruction address, specified by a symbol name and its offset from that
 * symbol's address.
 */
struct symbol_off_t {
  std::string symbol_name;
  ADDRINT offset;
};

/* Parse a start/stop point definition of symbol(+offset) and stores the
 * parsed symbol in result. Returns true on success, false otherwise.
 */
bool parse_sym(std::string sym_off, symbol_off_t& result);

VOID printMemoryUsage(THREADID tid);
VOID printElapsedTime();

#endif
