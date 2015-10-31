/* misc.cpp - miscellaneous routines
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "host.h"
#include "misc.h"

/* declare a fatal run-time error */
void _fatal(const char *file, const char *func, const int line, const char *fmt, ...) {
  va_list v;
  va_start(v, fmt);

  fprintf(stderr, "fatal: ");
  vfprintf(stderr, fmt, v);
  fprintf(stderr, " [%s:%s, line %d]\n", func, file, line);
  fflush(stderr);
  exit(1);
}

assert_fail_callback assert_fail;

void register_assert_fail_handler(assert_fail_callback callback) {
    assert_fail = callback;
}

/* The following are macros for basic memory operations.  If you have
   SSE support, these should run faster. */
void memzero(void * base, int bytes)
{
#ifdef USE_SSE_MOVE
  char * addr = (char*) base;

  asm ("xorps %%xmm0, %%xmm0"
       : : : "%xmm0");
  if (((uintptr_t)addr & 0x0f) == 0) /* aligned */
    for(int i=0;i<bytes>>4;i++)
    {
      asm ("movaps %%xmm0,  (%0)\n\t"
           : : "r"(addr) : "memory");
      addr += 16;
    }
  else /* unaligned */
    for(int i=0;i<bytes>>4;i++)
    {
      asm ("movlps %%xmm0,  (%0)\n\t"
           "movlps %%xmm0, 8(%0)\n\t"
           : : "r"(addr) : "memory");
      addr += 16;
    }

  // remainder
  for(int i=0;i<(bytes&0x0f);i++)
  {
    *addr = 0;
    addr++;
  }
#else
  memset(base,0,bytes);
#endif
}

/* swap contents of two memory buffers:
   SSE mode should be faster than equivalent memcpy
   version which would need to copy to a temporary
   memory buffer.  This version does the swap incrementally
   by making use of SSE/XMM registers for temp space.  Should
   reduce memory operations by about 33%. */
void memswap(void * p1, void * p2, size_t num_bytes)
{
  char * addr1 = (char*) p1;
  char * addr2 = (char*) p2;
#ifdef USE_SSE_MOVE
  int iter = num_bytes >> 4; // div by 16
  int rem = num_bytes & 0x0f; // mod 16
  char tmp[16];

  /* both buffers 16-byte aligned */
  if(((((uintptr_t)p1)&0x0f) | (((uintptr_t)p2)&0x0f)) == 0)
  {
    for(int i=0;i<iter;i++)
    {
      asm ("movaps (%0), %%xmm0\n\t" // load p1 into xmm0 and xmm1
           "movaps (%1), %%xmm1\n\t" // load p2 into xmm2 and xmm3
           "movaps %%xmm0, (%1)\n\t" // write p1 --> p2
           "movaps %%xmm1, (%0)\n\t" // write p2 --> p1
           : : "r"(addr1), "r"(addr2) : "memory","%xmm0","%xmm1");
      addr1 += 16;
      addr2 += 16;
    }
  }
  else /* unaligned */
  {
    for(int i=0;i<iter;i++)
    {
      asm ("movlps (%0),  %%xmm0\n\t" // load p1 into xmm0 and xmm1
           "movlps 8(%0), %%xmm1\n\t"
           "movlps (%1),  %%xmm2\n\t" // load p2 into xmm2 and xmm3
           "movlps 8(%1), %%xmm3\n\t"
           "movlps %%xmm0,  (%1)\n\t" // write p1 --> p2
           "movlps %%xmm1, 8(%1)\n\t"
           "movlps %%xmm2,  (%0)\n\t" // write p2 --> p1
           "movlps %%xmm3, 8(%0)\n\t"
           : : "r"(addr1), "r"(addr2) : "memory","%xmm0","%xmm1","%xmm2","%xmm3");
      addr1 += 16;
      addr2 += 16;
    }
  }
#else
  int rem = num_bytes;
  char * tmp = (char*)alloca(num_bytes);
#endif

  // any remaining bytes
  memcpy(tmp,addr1,rem);
  memcpy(addr1,addr2,rem);
  memcpy(addr2,tmp,rem);
  /*
  for(i=0;i<rem;i++)
  {
    char tmp = *addr1;
    *addr1 = *addr2;
    *addr2 = tmp;
    addr1++;
    addr2++;
  }*/
}

#ifdef GZIP_PATH

static struct {
  const char *type;
  const char *ext;
  const char *cmd;
} gzcmds[] = {
  /* type */	/* extension */		/* command */
  { "r",	".gz",			"%s -dc %s" },
  { "rb",	".gz",			"%s -dc %s" },
  { "r",	".Z",			"%s -dc %s" },
  { "rb",	".Z",			"%s -dc %s" },
  { "w",	".gz",			"%s > %s" },
  { "wb",	".gz",			"%s > %s" }
};

/* same semantics as fopen() except that filenames ending with a ".gz" or ".Z"
   will be automagically get compressed */
FILE *
gzopen(const char *fname, const char *type)
{
  const char *cmd = NULL;
  const char *ext;
  FILE *fd;
  char str[2048];

  /* get the extension */
  ext = mystrrchr(fname, '.');

  /* check if extension indicates compressed file */
  if (ext != NULL && *ext != '\0')
    {
      for (size_t i=0; i < sizeof(gzcmds) / sizeof(gzcmds[0]); i++)
	{
	  if (!strcmp(gzcmds[i].type, type) && !strcmp(gzcmds[i].ext, ext))
	    {
	      cmd = gzcmds[i].cmd;
	      break;
	    }
	}
    }

  if (!cmd)
    {
      /* open file */
      fd = fopen(fname, type);
    }
  else
    {
      /* open pipe to compressor/decompressor */
      sprintf(str, cmd, GZIP_PATH, fname);
      fd = popen(str, type);
    }

  return fd;
}

/* close compressed stream */
void
gzclose(FILE *fd)
{
  /* attempt pipe close, otherwise file close */
  if (pclose(fd) == -1)
    fclose(fd);
}

#else /* !GZIP_PATH */

FILE *
gzopen(const char *fname, const char *type)
{
  return fopen(fname, type);
}

void
gzclose(FILE *fd)
{
  fclose(fd);
}

#endif /* GZIP_PATH */
