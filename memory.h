/*
 * Handling of virtual-to-phisycal memory translation.
 * Copyright, Svilen Kanev, 2014
*/

#ifndef MEMORY_H
#define MEMORY_H

#include "host.h"

struct stat_sdb_t;

namespace xiosim {
namespace memory {

/* assuming 4KB pages */
const md_addr_t PAGE_SIZE = 4096;
const md_addr_t PAGE_SHIFT = 12; // log2(4K)
const md_addr_t PAGE_MASK = PAGE_SIZE - 1;

/* special address space id to indicate an already-translated address */
const int DO_NOT_TRANSLATE = -1;

/* initialize memory system  */
void init(int num_processes);

/* register memory system-specific statistics */
void reg_stats(struct stat_sdb_t *sdb);

/* map each (address-space-id,virtual-address) pair to a simulated physical address */
md_paddr_t v2p_translate(int asid, md_addr_t addr);

/* notify the virtual memory system of a non-speculative write.
 * This will allocate a new page if the page was unmapped. */
void notify_write(int asid, md_addr_t addr);

/* allocate physical pages that the simulated application requested */
void notify_mmap(int asid, md_addr_t addr, size_t length, bool mod_brk);

/* de-allocate physical pages released by the simulated application */
void notify_munmap(int asid, md_addr_t addr, size_t length, bool mod_brk);

/* update the brk point in address space @asid */
void update_brk(int asid, md_addr_t brk_end, bool do_mmap);

/* allocate physical pages for the stack of a current thread */
void map_stack(int asid, md_addr_t sp, md_addr_t bos);

/* Round @addr up to the nearest page. */
inline md_addr_t page_round_up(const md_addr_t addr) {
    return (addr + PAGE_MASK) & ~PAGE_MASK;
}

/* Round @addr down to the nearest page. */
inline md_addr_t page_round_down(const md_addr_t addr) {
    return addr & ~PAGE_MASK;
}

/* Get the offset of @addr in a page. */
inline md_addr_t page_offset(const md_addr_t addr) {
    return addr & PAGE_MASK;
}

/* this maps a virtual address to a low address where we're
   pretending that the page table information is stored.  The shift
   by PAGE_SHIFT yields the virtual page number, the masking is
   just to hash the address down to something that's less than the
   start of the .text section, and the additional offset is so that
   we don't generate a really low (e.g., NULL) address. */
inline md_addr_t page_table_address(const int asid, const md_addr_t addr) {
    return ((((addr >> PAGE_SHIFT) << 4) + asid) + 0x00080000) & 0x03ffffff;
    /* TODO(skanev): Check for 64b */
}

}
}

#endif /* MEMORY_H */
