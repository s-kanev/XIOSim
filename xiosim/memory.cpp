/*
 * Handling of virtual-to-phisycal memory translation.
 * Copyright, Svilen Kanev, 2014
*/

#include <assert.h>
#include <stdio.h>

#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <mutex>

#include "host.h"
#include "misc.h"
#include "sim.h"
#include "stats.h"
#include "synchronization.h"
#include "ztrace.h"

#include "memory.h"

namespace xiosim {
namespace memory {

/* VPN to PPN hash map. Note: these are page numbers, not addresses. */
typedef std::unordered_map<md_addr_t, md_paddr_t> page_table_t;

static int num_address_spaces;
static page_table_t * page_tables;
static md_addr_t * brk_point;

static counter_t * page_count;
static counter_t phys_page_count;

static XIOSIM_LOCK memory_lock;

/* given a set of pages, this creates a set of new page mappings. */
static void mem_newmap(int asid, md_addr_t addr, size_t length);
/* given a set of pages, this removes them from our map. */
static void mem_delmap(int asid, md_addr_t addr, size_t length);
/* check if a virtual address has been added to the mapping */
static bool mem_is_mapped(int asid, md_addr_t addr);
/* Get top of data segment */
static md_addr_t get_brk(int asid);
static void set_brk(int asid, md_addr_t brk);

void init(int num_processes)
{
    num_address_spaces = num_processes;

    page_tables = new page_table_t[num_processes];
    page_count = new counter_t[num_processes];
    brk_point = new md_addr_t[num_processes];

    memset(page_count, 0, num_processes * sizeof(page_count[0]));
    memset(brk_point, 0, num_processes * sizeof(brk_point[0]));
}

void deinit()
{
    delete[] brk_point;
    delete[] page_count;
    delete[] page_tables;
}

/* We allocate physical pages to virtual pages on a
 * first-come-first-serve basis. Seems like linux frowns upon
 * page coloring, so should be reasonably accurate. */
static md_paddr_t next_ppn_to_allocate = 0x00000100; /* arbitrary starting point; */
static void mem_newmap(int asid, md_addr_t addr, size_t length)
{
    ZTRACE_PRINT(INVALID_CORE, "mem_newmap: %d, %" PRIxPTR", length: %zd\n", asid, addr, length);

    assert(asid >= 0 && asid < num_address_spaces);
    assert(addr != 0); // Mapping 0-th page might cause hell to break loose, don't do it.

    /* Check alignment */
    if (page_offset(addr)) {
        fprintf(stderr, "mem_newmap: Address %" PRIxPTR" not aligned\n", addr);
        abort();
    }

    /* Add every page in the range to page table */
    md_addr_t last_addr = page_round_up(addr + length);
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

static void mem_delmap(int asid, md_addr_t addr, size_t length)
{
    ZTRACE_PRINT(INVALID_CORE, "mem_delmap: %d, %" PRIxPTR", length: %zd\n", asid, addr, length);

    assert(asid >= 0 && asid < num_address_spaces);

    /* Check alignment */
    if (page_offset(addr)) {
        fprintf(stderr, "mem_delmap: Address %" PRIxPTR" not aligned\n", addr);
        abort();
    }

    /* Remove every page in the range from page table */
    md_addr_t last_addr = page_round_up(addr + length);
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
static bool mem_is_mapped(int asid, md_addr_t addr)
{
    assert(asid >= 0 && asid < num_address_spaces);
    md_addr_t vpn = addr >> PAGE_SHIFT;
    return (page_tables[asid].count(vpn) > 0);
}

/* Get top of data segment */
static md_addr_t get_brk(int asid)
{
    assert(asid >= 0 && asid < num_address_spaces);
    return brk_point[asid];
}

/* Update top of data segment */
static void set_brk(int asid, md_addr_t brk_)
{
    assert(asid >= 0 && asid < num_address_spaces);
    brk_point[asid] = brk_;
}

md_paddr_t v2p_translate(int asid, md_addr_t addr)
{
    std::lock_guard<XIOSIM_LOCK> l(memory_lock);
    /* Some caches call this with an already translated address. Just ignore. */
    if (asid == DO_NOT_TRANSLATE)
        return addr;

    assert(asid >= 0 && asid < num_address_spaces);
    /* Page is mapped, just look it up */
    if (mem_is_mapped(asid, addr)) {
        md_addr_t vpn = addr >> PAGE_SHIFT;
        return (page_tables[asid][vpn] << PAGE_SHIFT) + page_offset(addr);
    }

    /* Else, return zeroth page and someone in higher layers will
     * complain if necessary */
    return 0 + page_offset(addr);
}

void notify_write(int asid, md_addr_t addr)
{
    std::lock_guard<XIOSIM_LOCK> l(memory_lock);
    if (!mem_is_mapped(asid, addr))
        mem_newmap(asid, page_round_down(addr), PAGE_SIZE);
}

void notify_mmap(int asid, md_addr_t addr, size_t length, bool mod_brk)
{
    std::lock_guard<XIOSIM_LOCK> l(memory_lock);
    md_addr_t page_addr = page_round_down(addr);
    size_t page_length = page_round_up(length);

    mem_newmap(asid, page_addr, page_length);

    md_addr_t curr_brk = get_brk(asid);
    if(mod_brk && page_addr > curr_brk)
        set_brk(asid, page_addr + page_length);
}

void notify_munmap(int asid, md_addr_t addr, size_t length, bool mod_brk)
{
    std::lock_guard<XIOSIM_LOCK> l(memory_lock);
    mem_delmap(asid, page_round_up(addr), length);
}

void update_brk(int asid, md_addr_t brk_end, bool do_mmap)
{
    assert(brk_end != 0);

    if(do_mmap)
    {
        md_addr_t old_brk_end = get_brk(asid);

        if(brk_end > old_brk_end)
            notify_mmap(asid, page_round_up(old_brk_end),
                        page_round_up(brk_end - old_brk_end), false);
        else if(brk_end < old_brk_end)
            notify_munmap(asid, page_round_up(brk_end),
                          page_round_up(old_brk_end - brk_end), false);
    }

    {
        std::lock_guard<XIOSIM_LOCK> l(memory_lock);
        set_brk(asid, brk_end);
    }
}

void map_stack(int asid, md_addr_t sp, md_addr_t bos)
{
    std::lock_guard<XIOSIM_LOCK> l(memory_lock);
    assert(sp != 0);
    assert(bos != 0);

    /* Create local pages for stack */
    md_addr_t page_start = page_round_down(sp);
    md_addr_t page_end = page_round_up(bos);

    mem_newmap(asid, page_start, page_end - page_start);
}

/* register memory system-specific statistics */
void reg_stats(xiosim::stats::StatsDatabase* sdb)
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
}
}
