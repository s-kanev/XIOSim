/* memory.c - flat memory space routines
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

#include <stdio.h>
#include <stdlib.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "options.h"
#include "stats.h"
#include "memory.h"
#include "synchronization.h"

extern bool multi_threaded;

/* create a flat memory space */
  struct mem_t *
mem_create(const char *name)			/* name of the memory space */
{
  struct mem_t *mem;

  mem = (struct mem_t*) calloc(1, sizeof(struct mem_t));
  if (!mem)
    fatal("out of virtual memory");

  mem->name = strdup(name);
  return mem;
}

/*************************************************************/
/* Code for providing simulated virtual-to-physical address
   translation.  This allocates physical pages to virtual pages on a
   first-come-first-serve basis.  In this fashion, pages from
   different cores/processes will be a little more distributed (more
   realistic), which also helps to reduce pathological aliasing
   effects (e.g., having all N cores' stacks map to the same exact
   cache sets). */
static md_paddr_t next_ppn_to_allocate = 0x00000100; /* arbitrary starting point; NOTE: this is a page number, not a page starting address */
#define V2P_HASH_SIZE 65536
#define V2P_HASH_MASK (V2P_HASH_SIZE-1)
struct v2p_node_t {
  int thread_id;
  md_addr_t vpn; /* virtual page number */
  md_paddr_t ppn; /* physical page number */
  struct v2p_node_t * next;
};

static struct v2p_node_t *v2p_hash_table[V2P_HASH_SIZE];

md_paddr_t v2p_translate(int thread_id, md_addr_t virt_addr)
{
  md_addr_t VPN = virt_addr >> PAGE_SHIFT;
  int index = VPN & (V2P_HASH_SIZE-1);
  struct v2p_node_t * p = v2p_hash_table[index], * prev = NULL;

  while(p)
  {
    if((p->vpn == VPN) &&
       (multi_threaded || 
        (!multi_threaded && (p->thread_id == thread_id))))
    {
      /* hit! */
      break;
    }
    prev = p;
    p = p->next;
  }

  if(!p) /* page miss: allocate a new physical page */
  {
    p = (struct v2p_node_t*) calloc(1,sizeof(*p));
    if(!p)
      fatal("couldn't calloc a new page table entry");
    p->thread_id = thread_id;
    p->vpn = VPN;
    p->ppn = next_ppn_to_allocate++;
    p->next = v2p_hash_table[index];

    v2p_hash_table[index] = p;
  }
  else if(prev)
  {
    /* move p to the front of the list (MRU) */
    prev->next = p->next;
    p->next = v2p_hash_table[index];
    v2p_hash_table[index] = p;
  }

  /* construct the final physical page number */
  return (p->ppn << PAGE_SHIFT) + (virt_addr & (PAGE_SIZE-1));
}

/* Wrapper around v2p_translate, to be called without holding the memory_lock */
md_paddr_t v2p_translate_safe(int thread_id, md_addr_t virt_addr)
{
  md_paddr_t res;
  lk_lock(&memory_lock, thread_id+1);
  res = v2p_translate(thread_id, virt_addr);
  lk_unlock(&memory_lock);
  return res;
}

/*************************************************************/


/* translate address ADDR in memory space MEM, returns pointer to host page */
  byte_t *
mem_translate(struct mem_t *mem,	/* memory space to access */
    md_addr_t addr)		/* virtual address to translate */
{
  struct mem_pte_t *pte, *prev;

  /* locate accessed PTE */
  for (prev=NULL, pte=mem->ptab[MEM_PTAB_SET(addr)];
      pte != NULL;
      prev=pte, pte=pte->next)
  {
    if (pte->tag == MEM_PTAB_TAG(addr))
    {
      /* move this PTE to head of the bucket list */
      if (prev)
      {
        prev->next = pte->next;
        pte->next = mem->ptab[MEM_PTAB_SET(addr)];
        mem->ptab[MEM_PTAB_SET(addr)] = pte;
      }

      return pte->page;
    }
  }

  /* no translation found, return NULL */
  return NULL;
}

// skumar - for implementing mmap syscall

/* given a set of pages, this creates a set of new page mappings,
 * try to use addr as the suggested addresses
 */
  md_addr_t
mem_newmap(struct mem_t *mem,            /* memory space to access */
    md_addr_t    addr,            /* virtual address to map to */
    size_t       length)
{
  int num_pages;
  int i;
  md_addr_t comp_addr;
  struct mem_pte_t *pte;
 
  /* first check alignment */
  if((addr & (MD_PAGE_SIZE-1))!=0) {
    fprintf(stderr, "mem_newmap address %x, not page aligned\n", addr);
    abort();
  }

  num_pages = length / MD_PAGE_SIZE + ((length % MD_PAGE_SIZE>0)? 1 : 0);
  for(i=0;i<num_pages;i++) {
    /* check if pages already allocated */
    comp_addr = addr+i*MD_PAGE_SIZE;
    if(mem_translate(mem, comp_addr)) {
#ifdef DEBUG
      if(debugging) 
        fprintf(stderr, "mem_newmap warning addr %x(page %d), already been allocated\n", comp_addr, i);
#endif
      // abort();
      continue;
    }

    /* generate a new PTE */
    pte = (struct mem_pte_t*) calloc(1, sizeof(struct mem_pte_t));
    if (!pte)
      fatal("out of virtual memory");
    pte->tag = MEM_PTAB_TAG(comp_addr);
    pte->addr = addr;
    pte->page = (byte_t*) comp_addr;

    /* insert PTE into inverted hash table */
    pte->next = mem->ptab[MEM_PTAB_SET(comp_addr)];
    mem->ptab[MEM_PTAB_SET(comp_addr)] = pte;

    /* one more page allocated */
    mem->page_count++;
  }
  return addr;

}

/* given a set of pages, delete them from the page table.
 */
  void
mem_delmap(struct mem_t *mem,            /* memory space to access */
    md_addr_t    addr,            /* virtual address to remove */
    size_t       length)	
{
  int num_pages;
  int i;
  md_addr_t comp_addr = addr;
  struct mem_pte_t *temp, *before_temp = NULL;

  /* first check alignment */
  if((addr & (MD_PAGE_SIZE-1))!=0) {
    fprintf(stderr, "mem_delmap address %x, not page aligned\n", addr);
    abort();
  }

  ZPIN_TRACE(INVALID_CORE, "Mem_delmap: %x, length: %x, end_addr: %x\n", addr, length, addr+length);

  num_pages = length / MD_PAGE_SIZE + ((length % MD_PAGE_SIZE>0)? 1 : 0);
  for(i=0;i<num_pages;i++) {
    /* check if pages already allocated */
    comp_addr = addr+i*MD_PAGE_SIZE;
    int set = MEM_PTAB_SET(comp_addr);

    if(!mem_translate(mem, comp_addr)) {
      // this is OK -- pin does this all the time.
      continue;
    }

    for (temp = mem->ptab[set] ; temp ; before_temp = temp, temp = temp->next)
    {
      if (temp->tag == MEM_PTAB_TAG(comp_addr))
      {
        if (before_temp)
        {
          before_temp->next = temp->next;
        }
        else
        {
          mem->ptab[set] = temp->next;
        }

        temp->page = NULL;
        temp->next = NULL;
        temp->tag = (md_addr_t)NULL;
        free( temp );

        /* one less page allocated */
        mem->page_count--;

        break;
      }
    }
  }
}

/* register memory system-specific statistics */
  void
mem_reg_stats(struct mem_t *mem,	/* memory space to declare */
    struct stat_sdb_t *sdb)	/* stats data base */
{
  char buf[512], buf1[512];

  stat_reg_note(sdb,"\n#### SIMULATED MEMORY STATS ####");
  sprintf(buf, "%s.page_count", mem->name);
  stat_reg_counter(sdb, TRUE, buf, "total number of pages allocated",
      &mem->page_count, 0, FALSE, NULL);

  sprintf(buf, "%s.page_mem", mem->name);
  sprintf(buf1, "%s.page_count * %d / 1024", mem->name, MD_PAGE_SIZE);
  stat_reg_formula(sdb, TRUE, buf, "total size of memory pages allocated",
      buf1, "%11.0fk");
}

/* initialize memory system */
  void
mem_init(struct mem_t *mem)	/* memory space to initialize */
{
  int i;

  /* initialize the first level page table to all empty */
  for (i=0; i < MEM_PTAB_SIZE; i++)
    mem->ptab[i] = NULL;

  mem->page_count = 0;
}
