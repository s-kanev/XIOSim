/*
 * Handling of virtual-to-phisycal memory translation.
 * Copyright, Svilen Kanev, 2014
*/

#ifndef MEMORY_H
#define MEMORY_H

#include "host.h"

/* assuming 4KB pages */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_OFFSET (PAGE_SIZE - 1)
/* compute address of access within a host page */
#define MEM_OFFSET(ADDR)  ((ADDR) & PAGE_OFFSET)

/* this maps a virtual address to a low address where we're
   pretending that the page table information is stored.  The shift
   by PAGE_SHIFT yields the virtual page number, the masking is
   just to hash the address down to something that's less than the
   start of the .text section, and the additional offset is so that
   we don't generate a really low (e.g., NULL) address. */
#define PAGE_TABLE_ADDR(T,A) ((((((A)>>PAGE_SHIFT)<<4)+(T)) + 0x00080000) & 0x03ffffff)
#define DO_NOT_TRANSLATE (-1)

struct stat_sdb_t;

namespace xiosim {
namespace memory {

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

}
}

#endif /* MEMORY_H */
