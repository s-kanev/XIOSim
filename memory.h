/* memory.h - flat memory space interfaces 
 *
 * SimpleScalar Ô Tool Suite
 * © 1994-2003 Todd M. Austin, Ph.D. and SimpleScalar, LLC
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING SIMPLESCALAR, YOU ARE AGREEING TO
 * THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted as
 * described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express or
 * implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged.  SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship purposes
 * provided that this notice in its entirety accompanies all copies. Copies of
 * the modified software can be delivered to persons who use it solely for
 * nonprofit, educational, noncommercial research, and noncommercial
 * scholarship purposes provided that this notice in its entirety accompanies
 * all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a copy
 * of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright © 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * Copyright © 2009 by Gabriel H. Loh and the Georgia Tech Research Corporation
 * Atlanta, GA  30332-0415
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING ZESTO, YOU ARE AGREEING TO THESE
 * TERMS AND CONDITIONS.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * NOTE: Portions of this release are directly derived from the SimpleScalar
 * Toolset (property of SimpleScalar LLC), and as such, those portions are
 * bound by the corresponding legal terms and conditions.  All source files
 * derived directly or in part from the SimpleScalar Toolset bear the original
 * user agreement.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Georgia Tech Research Corporation nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * 4. Zesto is distributed freely for commercial and non-commercial use.  Note,
 * however, that the portions derived from the SimpleScalar Toolset are bound
 * by the terms and agreements set forth by SimpleScalar, LLC.  In particular:
 * 
 *   "Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 *   downloaded, compiled, executed, copied, and modified solely for nonprofit,
 *   educational, noncommercial research, and noncommercial scholarship
 *   purposes provided that this notice in its entirety accompanies all copies.
 *   Copies of the modified software can be delivered to persons who use it
 *   solely for nonprofit, educational, noncommercial research, and
 *   noncommercial scholarship purposes provided that this notice in its
 *   entirety accompanies all copies."
 * 
 * User is responsible for reading and adhering to the terms set forth by
 * SimpleScalar, LLC where appropriate.
 * 
 * 5. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 6. Noncommercial and nonprofit users may distribute copies of Zesto in
 * compiled or executable form as set forth in Section 2, provided that either:
 * (A) it is accompanied by the corresponding machine-readable source code, or
 * (B) it is accompanied by a written offer, with no time limit, to give anyone
 * a machine-readable copy of the corresponding source code in return for
 * reimbursement of the cost of distribution. This written offer must permit
 * verbatim duplication by anyone, or (C) it is distributed by someone who
 * received only the executable form, and is accompanied by a copy of the
 * written offer of source code.
 * 
 * 7. Zesto was developed by Gabriel H. Loh, Ph.D.  US Mail: 266 Ferst Drive,
 * Georgia Institute of Technology, Atlanta, GA 30332-0765
 */


#ifndef MEMORY_H
#define MEMORY_H

#include <stdio.h>

#include "callbacks.h"
#include "host.h"
#include "misc.h"
#include "machine.h"
#include "options.h"
#include "stats.h"

/* number of entries in page translation hash table (must be power-of-two) */
#define MEM_PTAB_SIZE    (256*1024)
#define MEM_LOG_PTAB_SIZE  18
#define MAX_LEV                 3


/* log2(page_size) assuming 4KB pages */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/* this maps a virtual address to a low address where we're
   pretending that the page table information is stored.  The shift
   by PAGE_SHIFT yields the virtual page number, the masking is
   just to hash the address down to something that's less than the
   start of the .text section, and the additional offset is so that
   we don't generate a really low (e.g., NULL) address. */
#define PAGE_TABLE_ADDR(T,A) ((((((A)>>PAGE_SHIFT)<<4)+(T)) + 0x00080000) & 0x03ffffff)
#define DO_NOT_TRANSLATE (-1)


/* page table entry */
struct mem_pte_t {
  struct mem_pte_t *next;  /* next translation in this bucket */
  md_addr_t tag;    /* virtual page number tag */
  md_addr_t addr;   /* virtual address */
  int dirty;         /* has this page been modified? */
  byte_t *page;      /* page pointer */
  int from_mmap_syscall; /* TRUE if mapped from mmap/mmap2 call */

  /* these form a doubly-linked list through all pte's to track usage recency */
  struct mem_pte_t * lru_prev;
  struct mem_pte_t * lru_next;
};

/* just a simple container struct to track unused pages */
struct mem_page_link_t {
  byte_t * page;
  struct mem_page_link_t * next;
};

/* memory object */
struct mem_t {
  /* memory object state */
  char *name;        /* name of this memory space */
  struct mem_pte_t *ptab[MEM_PTAB_SIZE];/* inverted page table */
  struct mem_pte_t * recency_lru; /* oldest (LRU) */
  struct mem_pte_t * recency_mru; /* youngest (MRU) */

  counter_t page_count;      /* total number of pages allocated (in core and backing file) */
};

/*
 * virtual to host page translation macros
 */

/* compute page table set */
#define MEM_PTAB_SET(ADDR)            \
  (((ADDR) >> MD_LOG_PAGE_SIZE) & (MEM_PTAB_SIZE - 1))

/* compute page table tag */
#define MEM_PTAB_TAG(ADDR)            \
  ((ADDR) >> (MD_LOG_PAGE_SIZE + MEM_LOG_PTAB_SIZE))

/* convert a pte entry at idx to a block address */
#define MEM_PTE_ADDR(PTE, IDX)            \
  (((PTE)->tag << (MD_LOG_PAGE_SIZE + MEM_LOG_PTAB_SIZE))    \
   | ((IDX) << MD_LOG_PAGE_SIZE))

/* locate host page for virtual address ADDR, returns NULL if unallocated */
#define MEM_PAGE(MEM, ADDR, DIRTY)  mem_translate((MEM), (ADDR), (DIRTY))

/* compute address of access within a host page */
#define MEM_OFFSET(ADDR)  ((ADDR) & (MD_PAGE_SIZE - 1))

/* memory tickle function, allocates pages when they are first written */
#define MEM_TICKLE(MEM, ADDR)            \
  ((!MEM_PAGE(MEM, ADDR, 0)            \
    ? (/* allocate page at address ADDR */        \
      (void) mem_newmap(MEM, ROUND_DOWN(ADDR, MD_PAGE_SIZE), MD_PAGE_SIZE, 1)) \
    : (/* nada... */ (void)0)))           


/*
 * memory accessor macros
 */

/* these macros are to support speculative reads/writes of memory
   due to instructions in-flight (uncommitted) in the machine */

#define MEM_READ_BYTE(MEM, ADDR, MOP)          \
  core->oracle->spec_do_read_byte((ADDR), (MOP))

#define MEM_WRITE_BYTE(MEM, ADDR, VAL)          \
  core->oracle->spec_write_byte((ADDR),(VAL),uop)

/* Pin does the actual write if instruction is not speculative, but we do a 
   dummy address translate from the same page to update the MRU list and dirty flag in the page table */
#define MEM_WRITE_BYTE_NON_SPEC(MEM, ADDR, VAL)          \
  MEM_TICKLE(MEM, (md_addr_t)(ADDR));                   \
  MEM_PAGE(MEM, (md_addr_t)(ADDR),1)

#define MEM_DO_WRITE_BYTE_NON_SPEC(MEM, ADDR, VAL)          \
  (MEM_TICKLE(MEM, (md_addr_t)(ADDR)))

/* create a flat memory space */
struct mem_t *
mem_create(const char *name);      /* name of the memory space */

/* translate address ADDR in memory space MEM, returns pointer to host page */
byte_t *
mem_translate(struct mem_t *mem,  /* memory space to access */
    md_addr_t addr,     /* virtual address to translate */
    int dirty); /* should this page be marked as dirty? */

/* given a set of pages, this creates a set of new page mappings,
 * try to use addr as the suggested addresses
 */
md_addr_t
mem_newmap(struct mem_t *mem,           /* memory space to access */
    md_addr_t    addr,            /* virtual address to map to */
    size_t       length,
    int mmap);

/* given a set of pages, this removes them from our map.  Necessary
 * for munmap.
 */
void
mem_delmap(struct mem_t *mem,           /* memory space to access */
    md_addr_t    addr,            /* virtual address to delete */
    size_t       length);


/* register memory system-specific statistics */
void
mem_reg_stats(struct mem_t *mem,  /* memory space to declare */
    struct stat_sdb_t *sdb);  /* stats data base */

/* initialize memory system  */
void
mem_init(struct mem_t *mem);  /* memory space to initialize */

/* maps each (core-id,virtual-address) pair to a simulated physical address */
md_paddr_t v2p_translate(int core_id, md_addr_t virt_addr);
/* Wrapper around v2p_translate, to be called without holding the memory_lock */
md_paddr_t v2p_translate_safe(int thread_id, md_addr_t virt_addr);

#endif /* MEMORY_H */
