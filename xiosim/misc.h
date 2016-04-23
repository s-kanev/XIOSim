/* misc.h - miscellaneous interfaces
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
 * 4. Zesto is distributed freely for commercial and non-commercial use.
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

#ifndef MISC_H
#define MISC_H

#include <stdio.h>
#include <functional>

#include "core_const.h"

/* boolean value defs */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* declare a fatal run-time error */
#define fatal(fmt, ...)	\
  _fatal(__FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)

[[ noreturn ]] void _fatal(const char *file, const char *func, const int line, const char *fmt, ...);

typedef std::function<void(int coreID)> assert_fail_callback;
extern assert_fail_callback assert_fail;
void register_assert_fail_handler(assert_fail_callback callback);

#define xiosim_core_assert(cond, coreID) \
    if (!(cond)) {\
        fprintf(stderr,"assertion failed (%s:%d): %s\n", __FILE__, __LINE__, #cond); \
        fflush(stderr); \
        if (assert_fail) \
            assert_fail((coreID)); \
        exit(6); \
    }

#define xiosim_assert(cond) xiosim_core_assert((cond), xiosim::INVALID_CORE)

/* fast modulo increment/decrement:
   gcc on -O1 or higher will if-convert the following functions
   to provide much cheaper implementations of increment and
   decrement modulo some arbitrary value (not necessarily a
   power of two).  NOTE: argument x *must* be in the range
   0 .. (m-1), otherwise this code will not work! */

/* returns (x+1)%m */
inline int modinc(int x, int m)
{
  int xinc = x+1;
  if(xinc==m)
    xinc = 0;
  return xinc;
}

/* returns (x-1+m)%m */
inline int moddec(int x, int m)
{
  int xdec = x-1;
  if(x==0)
    xdec = m-1;
  return xdec;
}

/* returns x%m; NOTE: x must be in the range 0 .. (2m-1) */
inline int mod2m(int x, int m)
{
  int ret = x;
  if(x >= m)
    ret = ret - m;
  return ret;
}

/* Macro to annotate fallthrough labels, when supported (clang only for now) */
#ifdef __has_warning
#if __has_warning("-Wimplicit-fallthrough")
#define XIOSIM_FALLTHROUGH [[clang::fallthrough]]
#else
#define XIOSIM_FALLTHROUGH
#endif
#else
#define XIOSIM_FALLTHROUGH
#endif

/* fast memset macros - uses GCC's inline assembly */
void memzero(void * base, int bytes);
void memswap(void * p1, void * p2, size_t num_bytes);

/* same semantics as fopen() except that filenames ending with a ".gz" or ".Z"
   will be automagically get compressed */
FILE *gzopen(const char *fname, const char *type);

/* close compressed stream */
void gzclose(FILE *fd);

#endif /* MISC_H */
