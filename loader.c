/* loader.c - program loader routines
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
#include <sys/mman.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "endian.h"
#include "regs.h"
#include "memory.h"
#include "sim.h"
#include "eio.h"
#include "loader.h"
#include "stats.h"
#include "elf.h"

  /* amount of tail padding added to all loaded text segments */
#define TEXT_TAIL_PADDING 0 /* was: 128 */

  /* register simulator-specific statistics */
  void
ld_reg_stats(struct thread_t * thread, struct stat_sdb_t *sdb)	/* stats data base */
{
  char buf[128];

  stat_reg_note(sdb,"\n#### PROGRAM/TEXT STATS ####");
  sprintf(buf,"c%d.loader.text_base",thread->id);
  stat_reg_addr(sdb, TRUE, buf, "program text (code) segment base", &thread->loader.text_base, thread->loader.text_base, FALSE, "  0x%08p");
  sprintf(buf,"c%d.loader.text_bound",thread->id);
  stat_reg_addr(sdb, TRUE, buf, "program text (code) segment bound", &thread->loader.text_bound, thread->loader.text_bound, FALSE, "  0x%08p");
  sprintf(buf,"c%d.loader.text_size",thread->id);
  stat_reg_uint(sdb, TRUE, buf, "program text (code) size in bytes", &thread->loader.text_size, thread->loader.text_size, FALSE, NULL);
  sprintf(buf,"c%d.loader.data_base",thread->id);
  stat_reg_addr(sdb, TRUE, buf, "program initialized data segment base", &thread->loader.data_base, thread->loader.data_base, FALSE, "  0x%08p");
  sprintf(buf,"c%d.loader.data_bound",thread->id);
  stat_reg_addr(sdb, TRUE, buf, "program initialized data segment bound", &thread->loader.data_bound, thread->loader.data_bound, FALSE, "  0x%08p");
  sprintf(buf,"c%d.loader.data_size",thread->id);
  stat_reg_uint(sdb, TRUE, buf, "program init'ed `.data' and uninit'ed `.bss' size in bytes", &thread->loader.data_size, thread->loader.data_size, FALSE, NULL);
  sprintf(buf,"c%d.loader.stack_base",thread->id);
  stat_reg_addr(sdb, TRUE, buf, "program stack segment base (highest address in stack)", &thread->loader.stack_base, thread->loader.stack_base, FALSE, "  0x%08p");
#if 0 /* FIXME: broken... */
  stat_reg_addr(sdb, TRUE, "ld_stack_min", "program stack segment lowest address", &ld_stack_min, ld_stack_min, "0x%08p");
#endif
  sprintf(buf,"c%d.loader.stack_size",thread->id);
  stat_reg_uint(sdb, TRUE, buf, "program initial stack size", &thread->loader.stack_size, thread->loader.stack_size, FALSE, NULL);
  sprintf(buf,"c%d.loader.prog_entry",thread->id);
  stat_reg_addr(sdb, TRUE, buf, "program entry point (initial PC)", &thread->loader.prog_entry, thread->loader.prog_entry, FALSE, "  0x%08p");
  sprintf(buf,"c%d.loader.environ_base",thread->id);
  stat_reg_addr(sdb, TRUE, buf, "program environment base address address", &thread->loader.environ_base, thread->loader.environ_base, FALSE, "  0x%08p");
  sprintf(buf,"c%d.loader.target_big_endian",thread->id);
  stat_reg_int(sdb, TRUE, buf, "target executable endian-ness, non-zero if big endian", &thread->loader.target_big_endian, thread->loader.target_big_endian, FALSE, NULL);
}

#ifndef ZESTO_PIN

void elf_load_segment( FILE *fobj, struct mem_t *mem, md_addr_t addr, struct elf_phdr *phdr )
{
  int length = ROUND_UP(phdr->p_filesz + ELF_PAGEOFFSET(phdr->p_vaddr),MD_PAGE_SIZE) ;
  void *returnval = mmap( NULL, length,
      PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno( fobj ), 
      phdr->p_offset - ELF_PAGEOFFSET(phdr->p_vaddr) );
  mem_newmap2(mem, ELF_PAGESTART(addr), (md_addr_t) returnval, length, FALSE);
}

/* generate a random brk, just like FC3 */

unsigned long randomize_range(unsigned long start, unsigned long end, unsigned long len)
{
  unsigned long range = end - len - start;
  if (end <= start + len)
    return 0;
  return ROUND_UP((rand() % range + start), MD_PAGE_SIZE);
}


void randomize_brk(struct thread_t * thread)
{
  unsigned long new_brk, range_start, range_end;

  range_start = 0x08000000;
  if (thread->loader.brk_point >= range_start)
    range_start = thread->loader.brk_point;
  range_end = range_start + 0x02000000;
  new_brk = randomize_range(range_start, range_end, 0);
  if (new_brk)
    thread->loader.brk_point = new_brk;
}               

// map any pages that we need for the bss, and pad them with zeroes

void create_bss( struct thread_t *thread, unsigned long bss, unsigned long brk)
{
  struct mem_t * mem = thread->mem;
  unsigned long rounded_bss = ROUND_UP(bss,MD_PAGE_SIZE);
  unsigned long rounded_brk = ROUND_UP(brk,MD_PAGE_SIZE);
  unsigned long nbyte = ELF_PAGEOFFSET(bss);

  if (rounded_brk > rounded_bss)
  {
    unsigned long len = rounded_brk-rounded_bss;
    void *our_bss = mmap( NULL, len,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0 );
    mem_newmap2( mem, rounded_bss, (md_addr_t) our_bss, len, FALSE );
  }
  if (nbyte)
  {
    nbyte = MD_PAGE_SIZE - nbyte;
    void *buffer = calloc(nbyte,1);
    mem_bcopy(thread->id, mem_access, mem, Write, bss, buffer, nbyte);
  }
}

/* load program text and initialized data into simulated virtual memory
   space and initialize program segment range variables; returns 1 if
   program is an eio trace */
int
ld_load_prog(
    struct thread_t * thread,
    char *fname,		/* program to load */
    int argc, char **argv,	/* simulated program cmd line args */
    char **envp)		/* simulated program environment */
{
  int i;
  int id = thread->id;
  dword_t temp;
  struct regs_t * regs = &thread->regs;
  struct mem_t * mem = thread->mem;
  /*md_addr_t sp, data_break = 0, null_ptr = 0, argv_addr, envp_addr;*/
  md_addr_t sp, null_ptr = 0, argv_addr, envp_addr;
  FILE *fobj;
  struct elf_filehdr fhdr;
  struct elf_scnhdr shdr;
  byte_t *strtab, *buffer;
  int size;
  struct elf_phdr *phdata;
  md_addr_t start_code = ~0UL;
  md_addr_t end_code = 0;
  md_addr_t start_data = 0;
  md_addr_t end_data = 0;

  md_addr_t elf_bss = 0;
  md_addr_t elf_brk = 0;

  int load_addr_set = 0;
  unsigned long load_addr = 0;
  md_addr_t elf_aux_stuff[40];
  int ei_index = 0;
  int is_eio = FALSE;

  if (eio_valid(fname))
  {
    is_eio = TRUE;
    /* for multi-core support, you can now list multiple eio files */
#if 0
    if (argc != 1)
    {
      fprintf(stderr, "error: EIO file has arguments\n");
      exit(1);
    }
#endif

    fprintf(stderr, "sim: loading EIO file: %s\n", fname);

    sim_eio_fname[id] = mystrdup(fname);

    /* open the EIO file stream */
    sim_eio_fd[id] = eio_open(fname);

    /* load initial state checkpoint */
    if (eio_read_chkpt(thread, sim_eio_fd[id]) != -1)
      fatal("bad initial checkpoint in EIO file");

    /* computed state... */
    thread->loader.environ_base = regs->regs_R.dw[MD_REG_SP];
    thread->loader.prog_entry = regs->regs_PC;
    regs->regs_NPC = regs->regs_PC;

    /* fini... */
    return is_eio;
  }
#ifdef MD_CROSS_ENDIAN
  else
  {
    warn("endian of `%s' does not match host", fname);
    warn("running with experimental cross-endian execution support");
    warn("****************************************");
    warn("**>> please check results carefully <<**");
    warn("****************************************");
#if 0
    fatal("SimpleScalar/x86 only supports binary execution on\n"
        "       little-endian hosts, use EIO files on big-endian hosts");
#endif
  }
#endif /* MD_CROSS_ENDIAN */

  /* record profile file name */
  thread->loader.prog_fname = argv[0];

  /* load the program into memory, try both endians */
#if defined(__CYGWIN32__) || defined(_MSC_VER)
  fobj = fopen(argv[0], "rb");
#else
  fobj = fopen(argv[0], "r");
#endif
  if (!fobj)
    fatal("cannot open executable `%s'", argv[0]);

  if (fread(&fhdr, sizeof(struct elf_filehdr), 1, fobj) < 1)
    fatal("cannot read header from executable `%s'", argv[0]);

  /* check if it is a valid ELF file */
  if (!(fhdr.e_ident[EI_MAG0] == 0x7f &&
        fhdr.e_ident[EI_MAG1] == 'E' &&
        fhdr.e_ident[EI_MAG2] == 'L' &&
        fhdr.e_ident[EI_MAG3] == 'F'))
    fatal("bad magic number in executable `%s' (not an executable)", argv[0]);

  /* check if the file is executable */
  if (fhdr.e_type != ET_EXEC)
    fatal("object file `%s' is not executable", argv[0]);

  /* check if the file is for x86 architecture */
  if (fhdr.e_machine != EM_386)
    fatal("executable file `%s' is not for the x86 architecture", argv[0]);

  thread->loader.prog_entry = fhdr.e_entry;

  debug("number of sections in executable = %d\n", fhdr.e_shnum);
  debug("number of \"phnum\" in executable = %d\n", fhdr.e_phnum);
  debug("offset to section header table = %d", fhdr.e_shoff);

  /* seek to the beginning of the first section header, the file header comes
     first, followed by the optional header (this is the aouthdr), the size
     of the aouthdr is given in Fdhr.f_opthdr */
  fseek(fobj, fhdr.e_shoff + (fhdr.e_shstrndx * fhdr.e_shentsize), SEEK_SET);
  if (fread(&shdr, sizeof(struct elf_scnhdr), 1, fobj) < 1)
    fatal("error reading string section header from `%s'", argv[0]);

  /* allocate space for string table */
  strtab = (byte_t *)calloc(shdr.sh_size, sizeof(char));
  if (!strtab)
    fatal("out of virtual memory");

  /* read the string table */
  if (fseek(fobj, shdr.sh_offset, SEEK_SET) < 0)
    fatal("cannot seek to string table section");
  if (fread(strtab, shdr.sh_size, 1, fobj) < 0)
    fatal("cannot read string table section");

  debug("size of string table = %d", shdr.sh_size);
  debug("type of section = %d", shdr.sh_type);
  debug("offset of string table in file = %d", shdr.sh_offset);

  debug("processing %d sections in `%s'...", fhdr.e_shnum, argv[0]);

  /* loop through the section headers */
  for (i=0; i < fhdr.e_shnum; i++)
  {
    buffer = NULL;

    if (fseek(fobj, fhdr.e_shoff + (i * fhdr.e_shentsize), SEEK_SET) < 0)
      fatal("could not reset location in executable");
    if (fread(&shdr, sizeof(struct elf_scnhdr), 1, fobj) < 1)
      fatal("could not read section %d from executable", i);

    /* make sure the file is static */
    if (shdr.sh_type == SHT_DYNAMIC || shdr.sh_type == SHT_DYNSYM)
      fatal("file is dynamically linked, compile with `-static'");
  }

  // Okay, if we're here, the file is statically linked and we're good to go.
  // Let's load it by program headers rather than by sections; this is how
  // Linux does it, and it's how the ELF documentation says it should be done.

  size = fhdr.e_phnum * sizeof( struct elf_phdr );
  phdata = (elf_phdr*) calloc( 1, size );

  if (fseek(fobj, fhdr.e_phoff, SEEK_SET) < 0)
    fatal("could not reset location in executable");
  if (fread( phdata, size, 1, fobj ) < 1)
    fatal("could not read program headers" );

  for (i = 0 ; i < fhdr.e_phnum; i++)
  {
    struct elf_phdr *phdr = phdata + i;
    md_addr_t k = phdr->p_vaddr + phdr->p_filesz;
    md_addr_t vaddr = phdr->p_vaddr;

    // skip non-PT_LOAD sections
    if (phdr->p_type != PT_LOAD) continue; 

    if (elf_brk > elf_bss)
      fatal( "uh oh, robustify the loader please." );

    elf_load_segment( fobj, mem, vaddr, phdr );

    if (!load_addr_set)
    {
      load_addr_set = 1;
      load_addr = phdr->p_vaddr - phdr->p_offset;
    }

    if (phdr->p_vaddr < start_code) start_code = phdr->p_vaddr;
    if (phdr->p_vaddr > start_data) start_data = phdr->p_vaddr;

    if (k > elf_bss) elf_bss = k;
    if ((phdr->p_flags & PF_X) && end_code < k) end_code = k;
    if (end_data < k) end_data = k;
    k = phdr->p_vaddr + phdr->p_memsz;
    if (k > elf_brk) elf_brk = k;
  }

  thread->loader.text_base = start_code;
  thread->loader.text_bound = end_code;
  thread->loader.data_base = start_data;
  thread->loader.data_bound = end_data;
  thread->loader.data_size = thread->loader.data_bound - thread->loader.data_base;
  thread->loader.text_size = thread->loader.text_bound - thread->loader.text_base;

  create_bss( thread, elf_bss, elf_brk );

  /* release string table storage */
  free(strtab);

  /* perform sanity checks on segment ranges */
  if (!thread->loader.text_base || !thread->loader.text_size)
    fatal("executable is missing a `.text' section");
  if (!thread->loader.data_base || !thread->loader.data_size)
    fatal("executable is missing a `.data' section");
  if (!thread->loader.prog_entry)
    fatal("program entry point not specified");

  /* determine byte/words swapping required to execute on this host */
  sim_swap_bytes = (endian_host_byte_order() != endian_target_byte_order(thread));
  if (sim_swap_bytes)
  {
#if 0 /* FIXME: disabled until further notice... */
    /* cross-endian is never reliable, why this is so is beyond the scope
       of this comment, e-mail me for details... (not sure who "me" is... skumar?) */
    fprintf(stderr, "sim: *WARNING*: swapping bytes to match host...\n");
    fprintf(stderr, "sim: *WARNING*: swapping may break your program!\n");
    /* #else */
    fatal("binary endian does not match host endian");
#endif
  }
  sim_swap_words = (endian_host_word_order() != endian_target_word_order(thread));
  if (sim_swap_words)
  {
#if 0 /* FIXME: disabled until further notice... */
    /* cross-endian is never reliable, why this is so is beyond the scope
       of this comment, e-mail me for details... (not sure who "me" is... skumar?) */
    fprintf(stderr, "sim: *WARNING*: swapping words to match host...\n");
    fprintf(stderr, "sim: *WARNING*: swapping may break your program!\n");
    /* #else */
    fatal("binary endian does not match host endian");
#endif
  }

  /* set up a local stack pointer, this is where the argv and envp
     data is written into program memory */
  /* ld_stack_base = ld_text_base - (409600+4096); */

  thread->loader.stack_base = 0xc0000000;
#if 0
  sp = ROUND_DOWN(ld_stack_base - MD_MAX_ENVIRON, sizeof(MD_DOUBLE_TYPE));
#endif
  sp = thread->loader.stack_base - MD_MAX_ENVIRON;
  thread->loader.stack_size = thread->loader.stack_base - sp;

  /* initial stack pointer value */
  thread->loader.environ_base = sp;

  /* write [argc] to stack */
  temp = argc;
  mem_access(id, mem, Write, sp, &temp, sizeof(dword_t));
  sp += sizeof(dword_t);

  /* skip past argv array and NULL */
  argv_addr = sp;
  sp = sp + (argc + 1) * sizeof(md_addr_t);

  /* save space for envp array and NULL */
  envp_addr = sp;
  for (i=0; envp[i]; i++)
    sp += sizeof(md_addr_t);
  sp += sizeof(md_addr_t);

  memset( elf_aux_stuff, 0, sizeof(elf_aux_stuff) );

#define ADD_AUX( id, val  )\
  do { 	\
    elf_aux_stuff[ei_index++] = id;  \
    elf_aux_stuff[ei_index++] = val; \
  } while (0)
  ADD_AUX(AT_HWCAP, 0); 
  ADD_AUX(AT_PAGESZ, MD_PAGE_SIZE );
  ADD_AUX(AT_CLKTCK, 100);
  ADD_AUX(AT_PHDR, load_addr + fhdr.e_phoff);
  ADD_AUX(AT_PHENT, sizeof (struct elf_phdr));
  ADD_AUX(AT_PHNUM, fhdr.e_phnum);
  ADD_AUX(AT_BASE, 0); /* no interpreter */
  ADD_AUX(AT_FLAGS, 0);
  ADD_AUX(AT_ENTRY, thread->loader.prog_entry );
  ADD_AUX(AT_UID, (md_addr_t) getuid() );
  ADD_AUX(AT_EUID, (md_addr_t) geteuid() );
  ADD_AUX(AT_GID, (md_addr_t) getgid() );
  ADD_AUX(AT_EGID, (md_addr_t) getegid() );

  mem_bcopy(id, mem_access, mem, Write, sp, elf_aux_stuff, sizeof( elf_aux_stuff ) );
  sp += sizeof( elf_aux_stuff );

  /* fill in the argv pointer array and data */
  for (i=0; i<argc; i++)
  {
    /* write the argv pointer array entry */
    temp = sp;
    mem_access(id, mem, Write, argv_addr + i*sizeof(md_addr_t),
        &temp, sizeof(md_addr_t));
    /* and the data */
    mem_strcpy(id, mem_access, mem, Write, sp, argv[i]);
    sp += strlen(argv[i])+1;
  }
  /* terminate argv array with a NULL */
  mem_access(id, mem, Write, argv_addr + i*sizeof(md_addr_t),
      &null_ptr, sizeof(md_addr_t));

  /* write envp pointer array and data to stack */
  for (i = 0; envp[i]; i++)
  {
    /* write the envp pointer array entry */
    temp = sp;
    mem_access(id, mem, Write, envp_addr + i*sizeof(md_addr_t),
        &temp, sizeof(md_addr_t));
    /* and the data */
    mem_strcpy(id, mem_access, mem, Write, sp, envp[i]);
    sp += strlen(envp[i]) + 1;
  }
  /* terminate the envp array with a NULL */
  mem_access(id, mem, Write, envp_addr + i*sizeof(md_addr_t),
      &null_ptr, sizeof(md_addr_t));


  /* did we tromp off the stop of the stack? */
  if (sp > thread->loader.stack_base)
  {
    /* we did, indicate to the user that MD_MAX_ENVIRON must be increased,
       alternatively, you can use a smaller environment, or fewer
       command line arguments */
    fatal("environment overflow, increase MD_MAX_ENVIRON in x86.h");
  }

  /* initialize the bottom of heap to top of data segment */
  thread->loader.brk_point = ROUND_UP(elf_brk, MD_PAGE_SIZE);

  randomize_brk(thread);

  /* set initial minimum stack pointer value to initial stack value */
  thread->loader.stack_min = regs->regs_R.dw[MD_REG_ESP];

  regs->regs_R.dw[MD_REG_ESP] = thread->loader.environ_base;

  regs->regs_PC = thread->loader.prog_entry;
  regs->regs_NPC = regs->regs_PC;

  debug("ld_text_base: 0x%08x  ld_text_size: 0x%08x\n",
      thread->loader.text_base, thread->loader.text_size);
  debug("ld_data_base: 0x%08x  ld_data_size: 0x%08x\n",
      thread->loader.data_base, thread->loader.data_size);
  debug("ld_stack_base: 0x%08x  ld_stack_size: 0x%08x\n",
      thread->loader.stack_base, thread->loader.stack_size);
  debug("ld_prog_entry: 0x%08x\n", thread->loader.prog_entry);

  return is_eio;
}



/* reset program execution to beginning of EIO trace.  Used for looping
   execution of completed threads in multi-core mode. */
void
ld_reload_prog(struct thread_t * thread)
{
  int id = thread->id;
  struct regs_t * regs = &thread->regs;

  thread->stat.num_insn = thread->stat.num_refs =
    thread->stat.num_loads = thread->stat.num_branches = 0;
  /* TODO: clear all of memory, delete tmpfile */

  eio_close(sim_eio_fd[id]);

  sim_eio_fd[id] = eio_open(sim_eio_fname[id]);

  /* load initial state checkpoint */
  if (eio_read_chkpt(thread, sim_eio_fd[id]) != -1)
    fatal("bad initial checkpoint in EIO file");

  /* computed state... */
  thread->loader.environ_base = regs->regs_R.dw[MD_REG_SP];
  thread->loader.prog_entry = regs->regs_PC;
  regs->regs_NPC = regs->regs_PC;
}

#endif //ZESTO_PIN
