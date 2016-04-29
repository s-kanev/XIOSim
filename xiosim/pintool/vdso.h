#ifndef __FEEDER_VDSO_H__
#define __FEEDER_VDSO_H__

/* Utilities to parse VDSO symbols. Taken from the kernel docs. */

#include <utility>

/* Get VDSO base address from the auxv. */
uintptr_t vdso_addr();
#ifdef WE_DONT_NEED_NO_STINKING_LIBC
extern uintptr_t auxv_start;
#endif

/* Parse VDSO ELF dynamic symbol tables. Not thread-safe. */
void vdso_init_from_sysinfo_ehdr(uintptr_t base);

/* Search for a VDSO symbol. Returns (addr, length). */
std::pair<uintptr_t, size_t> vdso_sym(const char* name);

#endif /* __FEEDER_VDSO_H__ */
