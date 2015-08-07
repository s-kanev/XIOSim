/*
 * x86.h - x86 ISA definitions
 *
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
 *
 * This source file is distributed "as is" in the hope that it will be
 * useful.  The tool set comes with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use. 
 * 
 * Everyone is granted permission to copy, modify and redistribute
 * this tool set under the following conditions:
 * 
 *    This source code is distributed for non-commercial use only. 
 *    Please contact the maintainer for restrictions applying to 
 *    commercial use.
 *
 *    Permission is granted to anyone to make or distribute copies
 *    of this source code, either as received or modified, in any
 *    medium, provided that all copyright notices, permission and
 *    nonwarranty notices are preserved, and that the distributor
 *    grants the recipient permission for further redistribution as
 *    permitted by this document.
 *
 *    Permission is granted to distribute this file in compiled
 *    or executable form under the same conditions that apply for
 *    source code, provided that either:
 *
 *    A. it is accompanied by the corresponding machine-readable
 *       source code,
 *    B. it is accompanied by a written offer, with no time limit,
 *       to give anyone a machine-readable copy of the corresponding
 *       source code in return for reimbursement of the cost of
 *       distribution.  This written offer must permit verbatim
 *       duplication by anyone, or
 *    C. it is distributed by someone who received only the
 *       executable form, and is accompanied by a copy of the
 *       written offer of source code that they received concurrently.
 *
 * In other words, you are welcome to use, share and improve this
 * source file.  You are forbidden to forbid anyone else to use, share
 * and improve what you give them.
 *
 * INTERNET: dburger@cs.wisc.edu
 * US Mail:  1210 W. Dayton Street, Madison, WI 53706
 *
 */

#ifndef X86_H
#define X86_H

#include <cstdint>

extern "C" {

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "host.h"
#include "misc.h"


/*
 * This file contains various definitions needed to decode, disassemble, and
 * execute x86 instructions.
 */

/* build for x86 target */
const int MAX_CORES = 16;
const int INVALID_CORE = -1;


/*
 * target-dependent type definitions
 */

/* address type definition (32-bit) */
typedef dword_t md_addr_t;

/* physical address type definition (64-bit) */
typedef qword_t md_paddr_t;

struct inst_flags_t {
    bool CTRL:1;     /* control inst */
    bool UNCOND:1;   /*   unconditional change */
    bool COND:1;     /*   conditional change */
    bool MEM:1;      /* memory access inst */
    bool LOAD:1;     /*   load inst */
    bool STORE:1;    /*   store inst */
    bool TRAP:1;     /* traping inst */
    bool INDIR:1;    /* indirect control inst */
    bool CALL:1;     /* function call */
    bool RETN:1;     /* subroutine return */
};

/* TODO(skanev): These seem to be the only uop flags the timing model
 * currently uses. Figure out what to do about them. */
#define F_FCOMP 0x1

/* helper macros */

/*
 * various other helper macros/functions
 */

/* globbing/fusion masks */
#define FUSION_NONE 0x0000LL
#define FUSION_LOAD_OP 0x0001LL
#define FUSION_STA_STD 0x0002LL
#define FUSION_PARTIAL 0x0004LL  /* for partial-register-write merging uops */
/* to add: OP_OP, OP_ST */
#define FUSION_LD_OP_ST 0x0008LL /* for atomic Mop execution */
#define FUSION_FP_LOAD_OP 0x0010LL /* same as load op, but for fp ops */

namespace xiosim {
namespace x86 {
const size_t MAX_ILEN = 15;
}
}

//XXX: This should go away once I'm done with the cleanup
typedef struct {
    size_t len;
    bool rep;
    uint8_t code[xiosim::x86::MAX_ILEN];
} md_inst_t;

}

#endif /* X86_H */



