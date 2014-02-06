/*
 * Handling of virtual-to-phisycal memory translation.
 * Copyright, Svilen Kanev, 2014
*/

#ifndef MEMORY_H
#define MEMORY_H

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

/* initialize memory system  */
void mem_init(int num_processes);
/* register memory system-specific statistics */
void mem_reg_stats(struct stat_sdb_t *sdb);

/* given a set of pages, this creates a set of new page mappings. */
void mem_newmap(int asid, md_addr_t addr, size_t length);
/* given a set of pages, this removes them from our map. */
void mem_delmap(int asid, md_addr_t addr, size_t length);

/* check if a virtual address has been added to the mapping */
bool mem_is_mapped(int asid, md_addr_t addr);
/* maps each (core-id,virtual-address) pair to a simulated physical address */
md_paddr_t v2p_translate(int asid, md_addr_t addr);
/* wrapper around v2p_translate, to be called without holding the memory_lock */
md_paddr_t v2p_translate_safe(int asid, md_addr_t addr);

/* Get top of data segment */
md_addr_t mem_brk(int asid);
/* Update top of data segment */
void mem_update_brk(int asid, md_addr_t brk);

/* memory tickle function, allocates pages when they are first written */
#define MEM_TICKLE(ASID, ADDR)            \
  ((!mem_is_mapped((ASID), (ADDR))            \
    ? (/* allocate page at address ADDR */        \
      (void) mem_newmap(ASID, ROUND_DOWN(ADDR, PAGE_SIZE), PAGE_SIZE)) \
    : (/* nada... */ (void)0)))           


/*
 * memory accessor macros
 * XXX: These are obsolete and should go soon.
 */

/* these macros are to support speculative reads/writes of memory
   due to instructions in-flight (uncommitted) in the machine */

#define MEM_READ_BYTE(ADDR, MOP)          \
  core->oracle->spec_do_read_byte((ADDR), (MOP))

#define MEM_WRITE_BYTE(ADDR, VAL)          \
  core->oracle->spec_write_byte((ADDR),(VAL),uop)

/* Pin does the actual write if instruction is not speculative, but we do a 
   dummy address translate from the same page to update the MRU list in the page table */
#define MEM_WRITE_BYTE_NON_SPEC(ASID, ADDR, VAL)          \
  MEM_TICKLE(ASID, (md_addr_t)(ADDR))

#define MEM_DO_WRITE_BYTE_NON_SPEC(MEM, ADDR, VAL)          \
  (MEM_TICKLE(ASID, (md_addr_t)(ADDR)))

#endif /* MEMORY_H */
