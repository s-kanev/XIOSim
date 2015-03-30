/*
 * Handling of virtual-to-phisycal memory translation.
 * Copyright, Svilen Kanev, 2014
*/

#include <stdio.h>

#include <cstdlib>
#include <unordered_map>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "stats.h"
#include "memory.h"
#include "synchronization.h"

/* VPN to PPN hash map. Note: these are page numbers, not addresses. */
typedef std::unordered_map<md_addr_t, md_paddr_t> page_table_t;

static int num_address_spaces;
static page_table_t * page_tables;
static md_addr_t * brk_point;

static counter_t * page_count;
static counter_t phys_page_count;

void mem_init(int num_processes)
{
    num_address_spaces = num_processes;

    page_tables = new page_table_t[num_processes];
    page_count = new counter_t[num_processes];
    brk_point = new md_addr_t[num_processes];

    memset(page_count, 0, num_processes * sizeof(page_count[0]));
    memset(brk_point, 0, num_processes * sizeof(brk_point[0]));
}

/* We allocate physical pages to virtual pages on a
 * first-come-first-serve basis. Seems like linux frowns upon
 * page coloring, so should be reasonably accurate. */
static md_paddr_t next_ppn_to_allocate = 0x00000100; /* arbitrary starting point; */ 
void mem_newmap(int asid, md_addr_t addr, size_t length)
{
    ZPIN_TRACE(INVALID_CORE, "mem_newmap: %d, %x, length: %x\n", asid, addr, length);

    assert(asid >= 0 && asid < num_address_spaces);
    assert(addr != 0); // Mapping 0-th page might cause hell to break loose, don't do it.

    /* Check alignment */
    if (MEM_OFFSET(addr)) {
        fprintf(stderr, "mem_newmap: Address %x not aligned\n", addr);
        abort();
    }

    /* Add every page in the range to page table */
    md_addr_t last_addr = ROUND_UP(addr + length, PAGE_SIZE);
    for (md_addr_t curr_addr = addr; (curr_addr <= last_addr) && curr_addr; curr_addr += PAGE_SIZE) {
        if (mem_is_mapped(asid, curr_addr))
            continue; /* Attempting to double-map is ok */

        md_addr_t curr_vpn = curr_addr >> PAGE_SHIFT;
        page_tables[asid][curr_vpn] = next_ppn_to_allocate;

        next_ppn_to_allocate++;

        page_count[asid]++;
        phys_page_count++;
    }
}

void mem_delmap(int asid, md_addr_t addr, size_t length)
{
    ZPIN_TRACE(INVALID_CORE, "mem_delmap: %d, %x, length: %x\n", asid, addr, length);

    assert(asid >= 0 && asid < num_address_spaces);

    /* Check alignment */
    if (MEM_OFFSET(addr)) {
        fprintf(stderr, "mem_delmap: Address %x not aligned\n", addr);
        abort();
    }

    /* Remove every page in the range from page table */
    md_addr_t last_addr = ROUND_UP(addr + length, PAGE_SIZE);
    for (md_addr_t curr_addr = addr; (curr_addr <= last_addr) && curr_addr; curr_addr += PAGE_SIZE) {
        if (!mem_is_mapped(asid, curr_addr))
            continue; /* Attempting to remove something missing is ok */

        md_addr_t curr_vpn = curr_addr >> PAGE_SHIFT;
        page_tables[asid].erase(curr_vpn);

        page_count[asid]--;
        phys_page_count--;
    }
}

/* Check if page has been touched in this address space */
bool mem_is_mapped(int asid, md_addr_t addr)
{
    assert(asid >= 0 && asid < num_address_spaces);
    md_addr_t vpn = addr >> PAGE_SHIFT;
    return (page_tables[asid].count(vpn) > 0);
}

md_paddr_t v2p_translate(int asid, md_addr_t addr)
{
    /* Some caches call this with an already translated address. Just ignore. */
    if (asid == DO_NOT_TRANSLATE)
        return addr;

    assert(asid >= 0 && asid < num_address_spaces);
    /* Page is mapped, just look it up */
    if (mem_is_mapped(asid, addr)) {
        md_addr_t vpn = addr >> PAGE_SHIFT;
        return (page_tables[asid][vpn] << PAGE_SHIFT) + MEM_OFFSET(addr);
    }

    /* Else, return zeroth page and someone in higher layers will
     * complain if necessary */
    return 0 + MEM_OFFSET(addr);
}

/* Wrapper around v2p_translate, to be called without holding the memory_lock */
md_paddr_t v2p_translate_safe(int asid, md_addr_t addr)
{
    /* Some caches call this with an already translated address. Just ignore. */
    if (asid == DO_NOT_TRANSLATE)
        return addr;

    md_paddr_t res;
    lk_lock(&memory_lock, 1);
    res = v2p_translate(asid, addr);
    lk_unlock(&memory_lock);
    return res;
}

/* Get top of data segment */
md_addr_t mem_brk(int asid)
{
    assert(asid >= 0 && asid < num_address_spaces);
    return brk_point[asid];
}

/* Update top of data segment */
void mem_update_brk(int asid, md_addr_t brk_)
{
    assert(asid >= 0 && asid < num_address_spaces);
    brk_point[asid] = brk_;
}

/* register memory system-specific statistics */
void mem_reg_stats(struct stat_sdb_t *sdb)
{
    char buf[512];

    stat_reg_note(sdb,"\n#### SIMULATED MEMORY STATS ####");

    stat_reg_counter(sdb, TRUE, "core_page_count", "total number of physical pages allocated",
        &phys_page_count, 0, FALSE, NULL);
    for (int i=0; i<num_address_spaces; i++) {
        sprintf(buf, "prog_%d.page_count", i);
        stat_reg_counter(sdb, TRUE, buf, "total number of pages allocated",
            (page_count + i), 0, FALSE, NULL);
    }
}
