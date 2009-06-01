/* eio.c - external interfaces to external I/O files
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "libexo/libexo.h"
#include "syscall.h"
#include "sim.h"
#include "endian.h"
#include "eio.h"


#define EIO_FILE_HEADER							\
  "/* This is a SimpleScalar EIO file - DO NOT MOVE OR EDIT THIS LINE! */\n"
/*
   EIO transaction format:

   (inst_count, pc,
    ... reg inputs ...
    [r2, r3, r4, r5, r6, r7],
    ... mem inputs ...
    ((addr, size, blob), ...)
    ... reg outputs ...
    [r2, r3, r4, r5, r6, r7],
    ... mem outputs ...
    ((addr, size, blob), ...)
   )
*/

/* EIO transaction count, i.e., number of last transaction completed */
static counter_t eio_trans_icnt[MAX_CORES];

FILE *
eio_create(struct thread_t * thread,char *fname)
{
  FILE *fd;
  struct exo_term_t *exo;
  int target_big_endian;

  eio_trans_icnt[thread->id] = -1; /* initialize counter */

  target_big_endian = (endian_host_byte_order() == endian_big);

  fd = gzopen(fname, "w");
  if (!fd)
    fatal("unable to create EIO file `%s'", fname);

  /* emit EIO file header */
  fprintf(fd, "%s\n", EIO_FILE_HEADER);
  fprintf(fd, "/* file_format: %d, file_version: %d, big_endian: %d */\n", 
	  MD_EIO_FILE_FORMAT, EIO_FILE_VERSION, thread->loader.target_big_endian);
  exo = exo_new(ec_list,
		exo_new(ec_integer, (exo_integer_t)MD_EIO_FILE_FORMAT),
		exo_new(ec_integer, (exo_integer_t)EIO_FILE_VERSION),
		exo_new(ec_integer, (exo_integer_t)target_big_endian),
		NULL);
  exo_print(exo, fd);
  exo_delete(exo);
  fprintf(fd, "\n\n");

  return fd;
}

FILE *
eio_open(char *fname)
{
  FILE *fd;
  struct exo_term_t *exo;
  dword_t file_format, file_version, big_endian, target_big_endian;

  target_big_endian = (endian_host_byte_order() == endian_big);

  fd = gzopen(fname, "r");
  if (!fd)
    fatal("unable to open EIO file `%s'", fname);

  /* read and check EIO file header */
  exo = exo_read(fd);
  if (!exo_parse(exo, "(iii)", &file_format, &file_version, &big_endian))
    fatal("could not read EIO file header of file `%s'", fname);
  exo_delete(exo);

  if (file_format != MD_EIO_FILE_FORMAT)
    fatal("EIO file `%s' has incompatible format", fname);

  if (file_version != EIO_FILE_VERSION)
    fatal("EIO file `%s' has incompatible version", fname);

  if (!!big_endian != !!target_big_endian)
    {
      warn("endian of `%s' does not match host", fname);
      warn("running with experimental cross-endian execution support");
      warn("****************************************");
      warn("**>> please check results carefully <<**");
      warn("****************************************");
    }

  return fd;
}

/* returns non-zero if file FNAME has a valid EIO header */
int
eio_valid(char *fname)
{
  FILE *fd;
  char buf[512];

  /* open possible EIO file */
  fd = gzopen(fname, "r");
  if (!fd)
  {
    fprintf(stderr,"gzopen failed");
    return FALSE;
  }

  /* read and check EIO file header */
  fgets(buf, 512, fd);

  /* check the header */
  if (strcmp(buf, EIO_FILE_HEADER))
  {
    return FALSE;
  }

  /* all done, close up file */
  gzclose(fd);

  /* else, has a valid header, go with it... */
  return TRUE;
}

/* prototype: definition can be found in libexo/exolex.c */
void yy_close_stream(FILE * stream);

void
eio_close(FILE *fd)
{
  yy_close_stream(fd);
  gzclose(fd);
}

/* check point current architected state to stream FD, returns
   EIO transaction count (an EIO file pointer) */
counter_t
eio_write_chkpt(
    struct thread_t * thread,
		FILE *fd)			/* stream to write to */
{
  int i;
  struct exo_term_t *exo;
  struct mem_pte_t *pte;
  struct regs_t *regs = &thread->regs;
	struct mem_t *mem = thread->mem;

  myfprintf(fd, "/* ** start checkpoint @ %n... */\n\n", eio_trans_icnt[thread->id]);

  myfprintf(fd, "/* EIO file pointer: %n... */\n", eio_trans_icnt[thread->id]);
  exo = exo_new(ec_integer, (exo_integer_t)eio_trans_icnt[thread->id]);
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  /* dump misc regs: icnt, PC, NPC, etc... */
  fprintf(fd, "/* misc regs icnt, PC, NPC, etc... */\n");
  exo = MD_MISC_REGS_TO_EXO(regs);
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  /* dump integer registers */
  fprintf(fd, "/* integer regs */\n");
  exo = exo_new(ec_list, NULL);
  for (i=0; i < MD_NUM_IREGS; i++)
    exo->as_list.head = exo_chain(exo->as_list.head, MD_IREG_TO_EXO(regs, i));
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  /* dump FP registers */
  fprintf(fd, "/* FP regs (integer format) */\n");
  exo = exo_new(ec_list, NULL);
  for (i=0; i < MD_NUM_FREGS; i++)
    exo->as_list.head = exo_chain(exo->as_list.head, MD_FREG_TO_EXO(regs, i));
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  /* dump segment registers */
  fprintf(fd, "/* Segment regs (integer format) */\n");
  exo = exo_new(ec_list, NULL);
  for (i=0; i < MD_NUM_SREGS; i++)
    exo->as_list.head = exo_chain(exo->as_list.head, MD_SREG_TO_EXO(regs, i));
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  fprintf(fd, "/* writing `%d' memory pages... */\n", (int)mem->page_count);
  exo = exo_new(ec_list,
		exo_new(ec_integer, (exo_integer_t)mem->page_count),
		exo_new(ec_address, (exo_address_t)thread->loader.brk_point),
		exo_new(ec_address, (exo_address_t)thread->loader.stack_min),
		NULL);
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  fprintf(fd, "/* text segment specifiers (base & size) */\n");
  exo = exo_new(ec_list,
		exo_new(ec_address, (exo_address_t)thread->loader.text_base),
		exo_new(ec_integer, (exo_integer_t)thread->loader.text_size),
		NULL);
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  fprintf(fd, "/* data segment specifiers (base & size) */\n");
  exo = exo_new(ec_list,
		exo_new(ec_address, (exo_address_t)thread->loader.data_base),
		exo_new(ec_integer, (exo_integer_t)thread->loader.data_size),
		NULL);
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  fprintf(fd, "/* stack segment specifiers (base & size) */\n");
  exo = exo_new(ec_list,
		exo_new(ec_address, (exo_address_t)thread->loader.stack_base),
		exo_new(ec_integer, (exo_integer_t)thread->loader.stack_size),
		NULL);
  exo_print(exo, fd);
  fprintf(fd, "\n\n");
  exo_delete(exo);

  /* visit all active memory pages, and dump them to the checkpoint file */
  MEM_FORALL(mem, i, pte)
    {
      /* dump this page... */
      exo = exo_new(ec_list,
		    exo_new(ec_address, (exo_address_t)MEM_PTE_ADDR(pte, i)),
		    exo_new(ec_blob, MD_PAGE_SIZE, pte->page),
		    NULL);
      exo_print(exo, fd);
      fprintf(fd, "\n\n");
      exo_delete(exo);
    }

  myfprintf(fd, "/* ** end checkpoint @ %n... */\n\n", eio_trans_icnt[thread->id]);

  return eio_trans_icnt[thread->id];
}

/* read check point of architected state from stream FD, returns
   EIO transaction count (an EIO file pointer) */
counter_t
eio_read_chkpt(struct thread_t * thread, FILE *fd)			/* stream to read */
{
  int i;
  counter_t trans_icnt;
  dword_t page_count;
  struct exo_term_t *exo, *elt;
  struct regs_t *regs = &thread->regs;
  struct mem_t *mem = thread->mem;

  /* read the EIO file pointer */
  exo = exo_read(fd);
  if (!exo_parse(exo, "I", &trans_icnt))
    fatal("could not read EIO file pointer");
  exo_delete(exo);

  /* read misc regs: icnt, PC, NPC, HI, LO, FCC */
  exo = exo_read(fd);
  MD_EXO_TO_MISC_REGS(exo, thread->stat.num_insn, regs);
  exo_delete(exo);

  /* read integer registers */
  exo = exo_read(fd);
  if (!exo || exo->ec != ec_list)
    fatal("could not read EIO integer regs");
  elt = exo->as_list.head;
  for (i=0; i < MD_NUM_IREGS; i++)
    {
      if (!elt)
	fatal("could not read EIO integer regs (too few)");
      
      if (elt->ec != ec_address)
	fatal("could not read EIO integer regs (bad value)");

      MD_EXO_TO_IREG(elt, regs, i);
      elt = elt->next;
    }
  if (elt != NULL)
    fatal("could not read EIO integer regs (too many)");
  exo_delete(exo);

  /* read FP registers */
  exo = exo_read(fd);
  if (!exo || exo->ec != ec_list)
    fatal("could not read EIO FP regs");
  elt = exo->as_list.head;
  for (i=0; i < MD_NUM_FREGS; i++)
    {
      if (!elt)
	fatal("could not read EIO FP regs (too few)");
	
      if (elt->ec != ec_address)
	fatal("could not read EIO FP regs (bad value)");
	
      MD_EXO_TO_FREG(elt, regs, i);
      elt = elt->next;
    }
  if (elt != NULL)
    fatal("could not read EIO FP regs (too many)");
  exo_delete(exo);

  /* read segment registers */
  exo = exo_read(fd);
  if (!exo || exo->ec != ec_list)
    fatal("could not read EIO segment regs");
  elt = exo->as_list.head;
  for (i=0; i < MD_NUM_SREGS; i++)
    {
      if (!elt)
	fatal("could not read EIO segment regs (too few)");
	
      if (elt->ec != ec_address)
	fatal("could not read EIO segment regs (bad value)");
	
      MD_EXO_TO_SREG(elt, regs, i);
      elt = elt->next;
    }
  if (elt != NULL)
    fatal("could not read EIO segment regs (too many)");
  exo_delete(exo);

  /* read the number of page defs, and memory config */
  exo = exo_read(fd);
  if (!exo_parse(exo, "(iaa)", &page_count, &thread->loader.brk_point, &thread->loader.stack_min))
    fatal("could not read EIO memory page count");
  exo_delete(exo);

  /* read text segment specifiers */
  exo = exo_read(fd);
  if (!exo_parse(exo, "(ai)", &thread->loader.text_base, &thread->loader.text_size))
    fatal("count not read EIO text segment specifiers");
  exo_delete(exo);

  /* read data segment specifiers */
  exo = exo_read(fd);
  if (!exo_parse(exo, "(ai)", &thread->loader.data_base, &thread->loader.data_size))
    fatal("count not read EIO text segment specifiers");
  exo_delete(exo);

  /* read stack segment specifiers */
  exo = exo_read(fd);
  if (!exo_parse(exo, "(ai)", &thread->loader.stack_base, &thread->loader.stack_size))
    fatal("count not read EIO text segment specifiers");
  exo_delete(exo);

  for (i=0; i < (int)page_count; i++)
    {
      int j;
      md_addr_t page_addr;
      struct exo_term_t *blob;

      /* read the page */
      exo = exo_read(fd);
      if (!exo
	  || exo->ec != ec_list
	  || !exo->as_list.head
	  || exo->as_list.head->ec != ec_address
	  || !exo->as_list.head->next
	  || exo->as_list.head->next->ec != ec_blob
	  || exo->as_list.head->next->next != NULL)
	fatal("could not read EIO memory page");
 
      page_addr = (md_addr_t)exo->as_list.head->as_address.val;
      blob = exo->as_list.head->next;

      /* write data to simulator memory */
      for (j=0; j < blob->as_blob.size; j++)
	{
	  byte_t val;

	  val = blob->as_blob.data[j];
	  /* unchecked access... */
	  MEM_WRITE_BYTE(mem, page_addr, val);
	  page_addr++;
	}
      exo_delete(exo);
    }

  return trans_icnt;
}

struct mem_rec_t {
  md_addr_t addr;
  unsigned size, maxsize;
  struct exo_term_t *exo;
  struct exo_term_t *blob;
};

/* reg recs */
static struct exo_term_t *input_regs[MAX_CORES];
static struct exo_term_t *output_regs[MAX_CORES];

/* input memory recs */
static struct exo_term_t *input_mem[MAX_CORES];
static struct mem_rec_t input_mem_rec[MAX_CORES];

/* output memory recs */
static struct exo_term_t *output_mem[MAX_CORES];
static struct mem_rec_t output_mem_rec[MAX_CORES];

static int seen_write[MAX_CORES];
static mem_access_fn local_mem_fn;

/* size of padding that can be filled on the end of a blob tail */
#define BLOB_TAIL_SIZE		256

/* tracing memory access function */
static enum md_fault_type
my_mem_fn(
    int thread_id,
    struct mem_t *mem,		/* memory space to access */
	  enum mem_cmd cmd,		/* Read (from sim mem) or Write */
	  md_addr_t addr,		/* target address to access */
	  void *vp,			/* host memory address to access */
	  int nbytes)			/* number of bytes to access */
{
  int i;
  unsigned char *p = (unsigned char *)vp;
  struct mem_rec_t *mem_rec = NULL;
  struct exo_term_t *mem_list = NULL;
  enum md_fault_type fault = md_fault_none;

  if (cmd == Read && seen_write[thread_id])
    fatal("Read after Write in eio_syscall()");

  if (cmd == Write)
    seen_write[thread_id] = TRUE;

  /* record the memory access */
  if (cmd == Read)
    {
      mem_rec = &input_mem_rec[thread_id];
      mem_list = input_mem[thread_id];
    }
  else if (cmd == Write)
    {
      mem_rec = &output_mem_rec[thread_id];
      mem_list = output_mem[thread_id];
    }
  else
    panic("bogus memory access command");

  /* perform the memory access, Read's first so we can probe *p for data */
  if (cmd == Read /* simulator memory */)
    fault = (*local_mem_fn)(thread_id, mem, cmd, addr, p, nbytes);

  /* the following freakish code simply coalesces subsequent reads and
     writes to memory into the same EXO blob structure, this greatly
     reduces the size of the EIO output files... */
  if (mem_rec->exo != NULL
      && (mem_rec->addr + mem_rec->size == addr)
      && (mem_rec->size + nbytes < mem_rec->maxsize))
    {
      /* add to last blob */
      for (i=0; i < nbytes; i++)
	mem_rec->blob->as_blob.data[mem_rec->size + i] = p[i];
      mem_rec->size += nbytes;
      mem_rec->blob->as_blob.size = mem_rec->size;
    }
  else
    {
      /* add to a new blob */
      mem_list->as_list.head =
	exo_chain(mem_list->as_list.head,
		  (mem_rec->exo =
		   exo_new(ec_list,
			   exo_new(ec_address, (exo_address_t)addr),
			   (mem_rec->blob =
			    exo_new(ec_blob, nbytes + BLOB_TAIL_SIZE, NULL)),
			   NULL)));
      for (i=0; i < nbytes; i++)
	mem_rec->blob->as_blob.data[i] = p[i];
      mem_rec->addr = addr;
      mem_rec->size = nbytes;
      mem_rec->maxsize = nbytes + BLOB_TAIL_SIZE;
      mem_rec->blob->as_blob.size = mem_rec->size;
    }

  /* perform the memory access */
  if (cmd == Write /* simulator memory */)
    fault = (*local_mem_fn)(thread_id, mem, cmd, addr, p, nbytes);

  return fault;
}

/* syscall proxy handler, with EIO tracing support, architect registers
   and memory are assumed to be precise when this function is called,
   register and memory are updated with the results of the system call */
void
eio_write_trace(
    struct thread_t * thread,
    FILE *eio_fd,			/* EIO stream file desc */
		mem_access_fn mem_fn,		/* generic memory accessor */
		md_inst_t inst)			/* system call inst */
{
  int i;
  int thread_id = thread->id;
  struct exo_term_t *exo;
  struct regs_t *regs = &thread->regs;

  /* write syscall register inputs ($r2..$r7) */
  input_regs[thread_id] = exo_new(ec_list, NULL);
  //for (i=MD_FIRST_IN_REG; i <= MD_LAST_IN_REG; i++)
  for (i=0; i < MD_NUM_ARCH_IREGS; i++)
    {
      input_regs[thread_id]->as_list.head =
	exo_chain(input_regs[thread_id]->as_list.head, MD_IREG_TO_EXO(regs, i));
    }

  /* initialize memory inputs */
  input_mem[thread_id] = exo_new(ec_list, NULL); input_mem_rec[thread_id].exo = NULL;
  output_mem[thread_id] = exo_new(ec_list, NULL); output_mem_rec[thread_id].exo = NULL;

  /* perform the system call, record inputs and outputs */
  seen_write[thread_id] = FALSE;
  local_mem_fn = mem_fn;

  if (sim_eio_fd[thread->id] != NULL)
    eio_read_trace(thread, sim_eio_fd[thread->id], my_mem_fn, inst);
  else
    {
      sys_syscall(thread, my_mem_fn, inst, FALSE);
    }

  /* write syscall breakpoint and register outputs ($r2..$r7) */
  output_regs[thread_id] = exo_new(ec_list, NULL);
  output_regs[thread_id]->as_list.head =
	exo_chain(output_regs[thread_id]->as_list.head,
		  exo_new(ec_address, (exo_address_t)thread->loader.brk_point));
  //for (i=MD_FIRST_OUT_REG; i <= MD_LAST_OUT_REG; i++)
  for (i=0; i < MD_NUM_ARCH_IREGS; i++)
    {
      output_regs[thread_id]->as_list.head =
	exo_chain(output_regs[thread_id]->as_list.head, MD_IREG_TO_EXO(regs, i));
    }

  /* write the whole enchilada to output stream */
  struct exo_term_t * exo_icnt = exo_new(ec_integer, (exo_integer_t)thread->stat.num_insn);
  struct exo_term_t * exo_PC = exo_new(ec_address, (exo_address_t)regs->regs_PC);
  exo = exo_new(ec_list, exo_icnt, exo_PC,
		input_regs[thread_id], input_mem[thread_id],
		output_regs[thread_id], output_mem[thread_id],
		NULL);
  exo_print(exo, eio_fd);
  fprintf(eio_fd, "\n\n");

  /* release input storage */
  exo_delete(exo);

  /* one more transaction processed */
  eio_trans_icnt[thread->id] = thread->stat.num_insn;
}

/* syscall proxy handler from an EIO trace, architect registers
   and memory are assumed to be precise when this function is called,
   register and memory are updated with the results of the sustem call */
void
eio_read_trace(
         struct thread_t * thread,
         FILE *eio_fd,			/* EIO stream file desc */
	       mem_access_fn mem_fn,		/* generic memory accessor */
	       md_inst_t inst)			/* system call inst */
{
  int i;
  int thread_id = thread->id;
  struct exo_term_t *exo, *exo_icnt, *exo_pc;
  struct exo_term_t *exo_inregs, *exo_inmem, *exo_outregs, *exo_outmem;
  struct exo_term_t *brkrec, *regrec, *memrec;
 struct regs_t *regs = &thread->regs;
 struct mem_t *mem = thread->mem;

  /* exit() system calls get executed for real... */
  if (MD_EXIT_SYSCALL(inst, regs))
    {
      sys_syscall(thread, mem_fn, inst, FALSE);
      panic("returned from exit() system call");
    }

  /* else, read the external I/O (EIO) transaction */
  exo = exo_read(eio_fd);

  /* one more transaction processed */
  eio_trans_icnt[thread->id] = thread->stat.num_insn;

  /* pull apart the EIO transaction (EXO format) */
  if (!exo
      || exo->ec != ec_list
      || !(exo_icnt = exo->as_list.head)
      || exo_icnt->ec != ec_integer
      || !(exo_pc = exo_icnt->next)
      || exo_pc->ec != ec_address
      || !(exo_inregs = exo_pc->next)
      || exo_inregs->ec != ec_list
      || !(exo_inmem = exo_inregs->next)
      || exo_inmem->ec != ec_list
      || !(exo_outregs = exo_inmem->next)
      || exo_outregs->ec != ec_list
      || !(exo_outmem = exo_outregs->next)
      || exo_outmem->ec != ec_list
      || exo_outmem->next != NULL)
    fatal("cannot read EIO transaction");

  /*
   * check the system call inputs
   */

  /* check ICNT input */
  if (thread->stat.num_insn != (counter_t)exo_icnt->as_integer.val)
  {
    warn("EIO trace inconsistency: ICNT mismatch (thread %d); off by %lld --> correcting to EIO's state",thread->id,thread->stat.num_insn-exo_icnt->as_integer.val);
    thread->stat.num_insn = exo_icnt->as_integer.val;
    //fatal("EIO trace inconsistency: ICNT mismatch (thread %d)",thread->id);
  }

  /* check PC input */
  if (regs->regs_PC != (md_addr_t)exo_pc->as_address.val)
  {
    regs->regs_PC = exo_pc->as_address.val;
    warn("EIO trace inconsistency: PC mismatch (thread %d) - correcting to EIO's state",thread->id);
    //fatal("EIO trace inconsistency: PC mismatch (thread %d)",thread->id);
  }

  /* check integer register inputs */
  //for (i=MD_FIRST_IN_REG, regrec=exo_inregs->as_list.head;
       //i <= MD_LAST_IN_REG; i++, regrec=regrec->next)
  for (i=0, regrec=exo_inregs->as_list.head;
       i < MD_NUM_ARCH_IREGS; i++, regrec=regrec->next)
    {
      if (!regrec || regrec->ec != ec_address)
	fatal("EIO trace inconsistency: missing input reg");

      if (MD_EXO_CMP_IREG(regrec, regs, i))
      {
        MD_EXO_TO_IREG(regrec, regs, i);
        fatal("EIO trace inconsistency: R[%d] input mismatch (thread %d)", i, thread_id);
      }
#ifdef VERBOSE
      fprintf(stderr, "** R[%d] checks out...\n", i);
#endif /* VERBOSE */
    }
  if (regrec != NULL)
    fatal("EIO trace inconsistency: too many input regs (thread %d)",thread_id);

  /* check memory inputs */
  for (memrec=exo_inmem->as_list.head; memrec != NULL; memrec=memrec->next)
    {
      md_addr_t loc, value;
      struct exo_term_t *addr, *blob;

      /* check the mem transaction format */
      if (!memrec
	  || memrec->ec != ec_list
	  || !(addr = memrec->as_list.head)
	  || addr->ec != ec_address
	  || !(blob = addr->next)
	  || blob->ec != ec_blob
	  || blob->next != NULL)
	fatal("EIO trace inconsistency: bad memory transaction");
      
      value = addr->as_address.val;

      for (loc = value, i=0; i < blob->as_blob.size; loc++,i++)
	{
	  unsigned char val;

	  /* was: val = MEM_READ_BYTE(loc); */
	  (*mem_fn)(thread_id, mem, Read, loc, &val, sizeof(unsigned char));

	  if (val != blob->as_blob.data[i])
    {
      val = blob->as_blob.data[i];
      (*mem_fn)(thread_id, mem, Write, loc, &val, sizeof(unsigned char));
	    warn("EIO trace inconsistency: addr 0x%08p input mismatch (thread %d) --> correct to EIO's state", loc, thread_id);
	    //warn("EIO trace inconsistency: addr 0x%08p input mismatch (thread %d)", loc, thread_id);
    }

#ifdef VERBOSE
	  myfprintf(stderr, "** 0x%08p checks out...\n", loc);
#endif /* VERBOSE */
	}

      /* simulate view'able I/O */
      if (MD_OUTPUT_SYSCALL(inst, regs))
	{
	  if (sim_progfd)
	    {
	      /* redirect program output to file */
	      fwrite(blob->as_blob.data, 1, blob->as_blob.size, sim_progfd);
	    }
	  else
	    {
	      /* write the output to stdout/stderr */
	      write(MD_STREAM_FILENO(regs),
		    blob->as_blob.data, blob->as_blob.size);
	    }
	}
    }

  /*
   * write system call outputs
   */

  /* adjust breakpoint */
  brkrec = exo_outregs->as_list.head;
  if (!brkrec || brkrec->ec != ec_address)
    fatal("EIO trace inconsistency: missing memory breakpoint");
  thread->loader.brk_point = (md_addr_t)brkrec->as_address.val;

  /* write integer register outputs */
  //for (i=MD_FIRST_OUT_REG, regrec=exo_outregs->as_list.head->next;
       //i <= MD_LAST_OUT_REG; i++, regrec=regrec->next)
  for (i=0, regrec=exo_outregs->as_list.head->next;
       i < MD_NUM_ARCH_IREGS; i++, regrec=regrec->next)
    {
      if (!regrec || regrec->ec != ec_address)
	fatal("EIO trace inconsistency: missing output reg");

      MD_EXO_TO_IREG(regrec, regs, i);

#ifdef VERBOSE
      fprintf(stderr, "** R[%d] written...\n", i);
#endif /* VERBOSE */
    }
  if (regrec != NULL)
    fatal("EIO trace inconsistency: too many output regs");

  /* write memory outputs */
  for (memrec=exo_outmem->as_list.head; memrec != NULL; memrec=memrec->next)
    {
      md_addr_t loc, value;
      struct exo_term_t *addr, *blob;

      /* check the mem transaction format */
      if (!memrec
	  || memrec->ec != ec_list
	  || !(addr = memrec->as_list.head)
	  || addr->ec != ec_address
	  || !(blob = addr->next)
	  || blob->ec != ec_blob
	  || blob->next != NULL)
	fatal("EIO trace icnonsistency: bad memory transaction");
      
      value = addr->as_address.val;

      for (loc=value, i=0; i < blob->as_blob.size; loc++,i++)
	{
	  /* was: MEM_WRITE_BYTE(loc, blob->as_blob.data[i]); */
	  (*mem_fn)(thread_id, mem, Write,
		    loc, &blob->as_blob.data[i], sizeof(unsigned char));

#ifdef VERBOSE
	  fprintf(stderr, "** 0x%08p written...\n", loc);
#endif /* VERBOSE */
	}
    }

  /* release the EIO EXO node */
  exo_delete(exo);
}

/* fast forward EIO trace EIO_FD to the transaction just after ICNT */
void
eio_fast_forward(struct thread_t * thread, FILE *eio_fd, counter_t icnt)
{
  struct exo_term_t *exo, *exo_icnt;

  do
    {
      /* read the next external I/O (EIO) transaction */
      exo = exo_read(eio_fd);

      if (!exo)
	fatal("could not fast forward to EIO checkpoint");

      /* one more transaction processed */
      eio_trans_icnt[thread->id] = icnt;

      /* pull apart the EIO transaction (EXO format) */
      if (!exo
	  || exo->ec != ec_list
	  || !(exo_icnt = exo->as_list.head)
	  || exo_icnt->ec != ec_integer)
	fatal("cannot read EIO transaction (during fast forward)");
    }
  while ((counter_t)exo_icnt->as_integer.val != icnt);

  /* found it! */
}
