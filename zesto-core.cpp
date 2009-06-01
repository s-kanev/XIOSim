/* zesto-core.cpp - Zesto core (single pipeline) class
 *
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
 *
 */

#include <stddef.h>
#include "zesto-core.h"


bool core_t::static_members_initialized = false;
struct core_t::uop_array_t * core_t::uop_array_pool[MD_MAX_FLOWLEN+2+1];
struct odep_t * core_t::odep_free_pool = NULL;
int core_t::odep_free_pool_debt = 0;
seq_t core_t::global_seq = 0;


/* CONSTRUCTOR */
core_t::core_t(const int core_id):
  knobs(NULL), current_thread(NULL), id(core_id),
  num_emergency_recoveries(0), last_emergency_recovery_count(0),
  oracle(NULL), fetch(NULL), decode(NULL), alloc(NULL),
  exec(NULL), commit(NULL), global_action_id(0)
{
  memzero(&memory,sizeof(memory));
  memzero(&stat,sizeof(stat));

  assert(sizeof(struct uop_array_t) % 16 == 0);

  if(!static_members_initialized)
  {
    memzero(uop_array_pool,sizeof(uop_array_pool));
    static_members_initialized = true;
  }
}

/* assign a new, unique id */
seq_t core_t::new_action_id(void)
{
  global_action_id++;
  return global_action_id;
}

/* Returns an array of uop structs; manages its own free pool by
   size.  The input to this function should be the Mop's uop flow
   length.  The implementation of this is slightly (very?) ugly; see
   the definition of struct uop_array_t.  We basically define a
   struct that contains a pointer for linking everything up in the
   free lists, but we don't actually want the outside world to know
   about these pointers, so we actually return a pointer that is !=
   to the original address returned by calloc.  If you're familiar
   with the original cache structures from the old SimpleScalar, we
   declare our uop_array_t similar to that. */
struct uop_t * core_t::get_uop_array(const int size)
{
  struct uop_array_t * p;

  if(uop_array_pool[size])
  {
    p = uop_array_pool[size];
    uop_array_pool[size] = p->next;
    p->next = NULL;
    assert(p->size == size);
  }
  else
  {
    //p = (struct uop_array_t*) calloc(1,sizeof(*p)+size*sizeof(struct uop_t));
    posix_memalign((void**)&p,16,sizeof(*p)+size*sizeof(struct uop_t)); // force all uops to be 16-byte aligned
    if(!p)
      fatal("couldn't calloc new uop array");
    p->size = size;
    p->next = NULL;
  }
  /* initialize the uop array */
  for(int i=0;i<size;i++)
    uop_init(&p->uop[i]);
  return p->uop;
}

void core_t::return_uop_array(struct uop_t * const p)
{
  struct uop_array_t * ap;
  byte_t * bp = (byte_t*)p;
  bp -= offsetof(struct uop_array_t,uop);
  ap = (struct uop_array_t *) bp;

  assert(ap->next == NULL);
  ap->next = uop_array_pool[ap->size];
  uop_array_pool[ap->size] = ap;
}

/* Alloc/dealloc of the linked-list container nodes */
struct odep_t * core_t::get_odep_link(void)
{
  struct odep_t * p = NULL;
  if(odep_free_pool)
  {
    p = odep_free_pool;
    odep_free_pool = p->next;
  }
  else
  {
    p = (struct odep_t*) calloc(1,sizeof(*p));
    if(!p)
      fatal("couldn't calloc an odep_t node");
  }
  assert(p);
  p->next = NULL;
  odep_free_pool_debt++;
  return p;
}

void core_t::return_odep_link(struct odep_t * const p)
{
  p->next = odep_free_pool;
  odep_free_pool = p;
  p->uop = NULL;
  odep_free_pool_debt--;
  /* p->next used for free list, will be cleared on "get" */
}

/* all sizes/loop lengths known at compile time; compiler
   should be able to optimize this pretty well.  Assumes
   uop is aligned to 16 bytes. */
void core_t::zero_uop(struct uop_t * const uop)
{
#if USE_SSE_MOVE
  char * addr = (char*) uop;
  int bytes = sizeof(*uop);
  int remainder = bytes - (bytes>>7)*128;

  /* zero xmm0 */
  asm ("xorps %%xmm0, %%xmm0"
       : : : "%xmm0");
  /* clear the uop 64 bytes at a time */
  for(int i=0;i<bytes>>7;i++)
  {
    asm ("movaps %%xmm0,    (%0)\n\t"
         "movaps %%xmm0,  16(%0)\n\t"
         "movaps %%xmm0,  32(%0)\n\t"
         "movaps %%xmm0,  48(%0)\n\t"
         "movaps %%xmm0,  64(%0)\n\t"
         "movaps %%xmm0,  80(%0)\n\t"
         "movaps %%xmm0,  96(%0)\n\t"
         "movaps %%xmm0, 112(%0)\n\t"
         : : "r"(addr) : "memory");
    addr += 128;
  }

  /* handle any remaining bytes; optimizer should remove this
     when sizeof(uop) has no remainder */
  for(int i=0;i<remainder>>3;i++)
  {
    asm ("movlps %%xmm0,   (%0)\n\t"
         : : "r"(addr) : "memory");
    addr += 8;
  }
#else
  memset(uop,0,sizeof(*uop));
#endif
}

void core_t::zero_Mop(struct Mop_t * const Mop)
{
#if USE_SSE_MOVE
  char * addr = (char*) Mop;
  int bytes = sizeof(*Mop);
  int remainder = bytes - (bytes>>6)*64;

  /* zero xmm0 */
  asm ("xorps %%xmm0, %%xmm0"
       : : : "%xmm0");
  /* clear the uop 64 bytes at a time */
  for(int i=0;i<bytes>>6;i++)
  {
    asm ("movaps %%xmm0,   (%0)\n\t"
         "movaps %%xmm0, 16(%0)\n\t"
         "movaps %%xmm0, 32(%0)\n\t"
         "movaps %%xmm0, 48(%0)\n\t"
         : : "r"(addr) : "memory");
    addr += 64;
  }

  /* handle any remaining bytes */
  for(int i=0;i<remainder>>3;i++)
  {
    asm ("movlps %%xmm0,   (%0)\n\t"
         : : "r"(addr) : "memory");
    addr += 8;
  }
#else
  memset(Mop,0,sizeof(*Mop));
#endif
}


/* Initialize a uop struct */
void core_t::uop_init(struct uop_t * const uop)
{
  int i;
  zero_uop(uop);
  memset(&uop->alloc,-1,sizeof(uop->alloc));
  uop->core = this;
  uop->decode.Mop_seq = (seq_t)-1;
  uop->decode.uop_seq = (seq_t)-1;
  uop->alloc.port_assignment = -1;

  uop->timing.when_decoded = TICK_T_MAX;
  uop->timing.when_allocated = TICK_T_MAX;
  for(i=0;i<MAX_IDEPS;i++)
  {
    uop->timing.when_itag_ready[i] = TICK_T_MAX;
    uop->timing.when_ival_ready[i] = TICK_T_MAX;
  }
  uop->timing.when_otag_ready = TICK_T_MAX;
  uop->timing.when_ready = TICK_T_MAX;
  uop->timing.when_issued = TICK_T_MAX;
  uop->timing.when_exec = TICK_T_MAX;
  uop->timing.when_completed = TICK_T_MAX;

  uop->exec.action_id = new_action_id();
  uop->exec.when_data_loaded = TICK_T_MAX;
  uop->exec.when_addr_translated = TICK_T_MAX;
}

