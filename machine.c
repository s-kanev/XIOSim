/* x86.c (machine.c) - x86 ISA definition routines
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
 */

#include <stdio.h>
#include <stdlib.h>

#include "sim.h"
#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "x86flow.def"
#include "zesto-structs.h"

#ifndef SYMCAT
#define SYMCAT(XX,YY)	XX ## YY
#endif

#ifndef SYMCAT3
#define SYMCAT3(XX,YY,ZZ)	XX ## YY ## ZZ
#endif

/* opcode mask -> enum md_opcodem, used by decoder */
enum md_opcode md_mask2op[MD_MAX_MASK+1];
unsigned int md_opoffset[OP_MAX];

/* enum md_opcode -> mask for decoding next level */
const unsigned int md_opmask[OP_MAX] = {
  0, /* NA */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) 0,
#define DEFUOP(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) 0,
#define DEFLINK(OP,MSK,NAME,SHIFT,MASK) MASK,
#define CONNECT(OP)
#include "machine.def"
};

/* enum md_opcode -> shift for decoding next level */
const unsigned int md_opshift[OP_MAX] = {
  0, /* NA */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) 0,
#define DEFUOP(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) 0,
#define DEFLINK(OP,MSK,NAME,SHIFT,MASK) SHIFT,
#define CONNECT(OP)
#include "machine.def"
};

/* enum md_opcode -> description string */
const char *md_op2name[OP_MAX] = {
  NULL, /* NA */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) NAME,
#define DEFUOP(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) NAME,
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT) NAME,
#define CONNECT(OP)
#include "machine.def"
};

/* enum md_opcode -> enum md_fu_class, used by performance simulators */
const enum md_fu_class md_op2fu[OP_MAX] = {
  FU_NA, /* NA */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) RES,
#define DEFUOP(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) RES,
#define DEFLINK(OP,MSK,NAME, MASK,SHIFT) FU_INVALID,
#define CONNECT(OP)
#include "machine.def"
};

/* enum md_fu_class -> description string */
const char *md_fu2name[NUM_FU_CLASSES] = {
  NULL, /* NA */
  "int-exec-unit",
  "jump-exec-unit",
  "int-multiply",
  "int-shift",
  "FP-add",
  "FP-multiply",
  "FP-complex",
  "FP-divide",
  "load-port",
  "sta-port",
  "std-port",
  "agen-unit",
  "magic-unit"
};

#define _OP		MODE_OPER32
#define _AD		MODE_ADDR32
#define PFX_MODE(X)	((X) & 0x07)

#define _CS		(SEG_CS << 3)
#define _SS		(SEG_SS << 3)
#define _DS		(SEG_DS << 3)
#define _ES		(SEG_ES << 3)
#define _FS		(SEG_FS << 3)
#define _GS		(SEG_GS << 3)
#define PFX_SEG(X)	(((X) >> 3) & 0x07)

#define _LK		(1 << 6)
#define PFX_LOCK(X)	(((X) >> 6) & 1)

#define _NZ		(REP_REPNZ << 7)
#define _Z		(REP_REPZ << 7)
#define PFX_REP(X)	(((X) >> 7) & 0x03)

const static word_t pfxtab[256] = {
  /* 0x00 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x08 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x10 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x18 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x20 */  0,   0,   0,   0,   0,   0, _ES,   0,
  /* 0x28 */  0,   0,   0,   0,   0,   0, _CS,   0,
  /* 0x30 */  0,   0,   0,   0,   0,   0, _SS,   0,
  /* 0x38 */  0,   0,   0,   0,   0,   0, _DS,   0,
  /* 0x40 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x48 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x50 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x58 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x60 */  0,   0,   0,   0, _FS, _GS, _OP, _AD,
  /* 0x68 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x70 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x78 */  0,   0,   0,   0,   0,   0,   0,   0,

  /* 0x80 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x88 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x90 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0x98 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xa0 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xa8 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xb0 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xb8 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xc0 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xc8 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xd0 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xd8 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xe0 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xe8 */  0,   0,   0,   0,   0,   0,   0,   0,
  /* 0xf0 */_LK,   0, _NZ,  _Z,   0,   0,   0,   0,
  /* 0xf8 */  0,   0,   0,   0,   0,   0,   0,   0
};


  int
md_cc_eval(const int cond, const dword_t aflags, bool * bogus)
{
  int res;

  switch (cond)
  {
    case CC_O:		res = !!(aflags & OF); break;
    case CC_NO:		res = !(aflags & OF); break;
    case CC_B:		res = !!(aflags & CF); break;
    case CC_NB:		res = !(aflags & CF); break;
    case CC_E:		res = !!(aflags & ZF); break;
    case CC_NE:		res = !(aflags & ZF); break;
    case CC_BE:		res = (aflags & CF) || (aflags & ZF); break;
    case CC_NBE:	res = !(aflags & CF) && !(aflags & ZF); break;
    case CC_S:		res = !!(aflags & SF); break;
    case CC_NS:		res = !(aflags & SF); break;
    case CC_P:		warnonce("PF flags probed"); res = !!(aflags & PF); break; // skumar - warn changed to warnonce
    case CC_NP:		warnonce("PF flags probed"); res = !(aflags & PF); break; 
    case CC_L:		res = !(aflags & SF) != !(aflags & OF); break;
    case CC_NL:		res = !(aflags & SF) == !(aflags & OF); break;
    case CC_LE:
                  res = (aflags & ZF) || (!(aflags & SF) != !(aflags & OF)); break;
    case CC_NLE:
                  res = !(aflags & ZF) && (!(aflags & SF) == !(aflags & OF)); break;
    default: if(bogus)
             {
               *bogus = TRUE;
               res = 0;
             }
             else
               fatal("bogus CC condition: %d", cond);
  }
  return res;
}

  int
md_fcc_eval(int cond, dword_t aflags, bool * bogus)
{
  int res;

  switch (cond)
  {
    case FCC_B:		res = !!(aflags & CF); break;
    case FCC_E:		res = !!(aflags & ZF); break;
    case FCC_BE:	res = ((aflags & CF) || (aflags & ZF)); break;
    case FCC_U:		res = !!(aflags & PF); break;
    case FCC_NB:	res = !(aflags & CF); break;
    case FCC_NE:	res = !(aflags & ZF); break;
    case FCC_NBE:	res = (!(aflags & CF) && !(aflags & ZF)); break;
    case FCC_NU:	res = !(aflags & PF); break;
    default: if(bogus)
             {
               *bogus = TRUE;
               res = 0;
             }
             else
               fatal("bogus FCC condition: %d", cond);
  }
  return res;
}

/* decode an instruction */
  enum md_opcode
md_decode(const byte_t mode, struct md_inst_t *inst, const int set_nop)
{
  byte_t *pinst = inst->code, modbyte;
  int pfx, npfx, nopc, modrm, flags, sib;
  int base, index, scale, ndisp, nimm;
  sdword_t disp, imm;
  enum md_opcode op;
  unsigned char real_op = 0;

  /* decode current execution mode */
  pfx = 0; npfx = 0;
  while (pfxtab[*pinst])
  {
    /* NOTE: multiple identical prefi (sp?) do not negate the previous */
    pfx |= pfxtab[*pinst++];
    npfx++;
  }
  inst->npfx = npfx;
  inst->mode = PFX_MODE(pfx) ^ mode; /* FIXME: swizzle in mode at runtime... */
  inst->rep = PFX_REP(pfx);
  inst->lock = PFX_LOCK(pfx);
  inst->seg = PFX_SEG(pfx);

  /* decode the instruction opcode */
  if (*pinst == 0x0f)
  {
    /* decode escape, advance to opcode */
    op = md_mask2op[*pinst++];
    /* decode opcode, advance to (possible) modrm */
    real_op = *pinst;
    op = md_mask2op[(((*pinst++ >> md_opshift[op]) & md_opmask[op])
        + md_opoffset[op])];
    nopc = 2;
  }
  else
  {
    /* decode opcode, advance to (possible) modrm */
    real_op = *pinst;
    op = md_mask2op[*pinst++];
    nopc = 1;
  }
  inst->nopc = nopc;

  /* decode possible modrm */
  modrm = 0;
  while (md_opmask[op])
  {
    real_op = *pinst;
    op = md_mask2op[(((*pinst >> md_opshift[op]) & md_opmask[op])
        + md_opoffset[op])];
    modrm = 1;
  }

  if (op == NA)
  {
    if ( set_nop == 0 )
      fatal("invalid opcode: 0x%x:%x", real_op,op);
    else 
      return MD_NOP_OP;

    inst->modrm = 0;
    inst->sib = 0;
    inst->base = -1;
    inst->index = -1;
    inst->ndisp = 0;
    inst->disp = 0;
    inst->nimm = 0;
    inst->imm = 0;
    inst->len = npfx + nopc + modrm;
    return op;
  }

  flags = MD_OP_FLAGS(op);

  /* adjust repeat */
  if (inst->rep == REP_REPZ)
  {
    if (flags & F_REP)
      inst->rep = REP_REP;
  }

  /* rep is ignored for non-string instrutions */
  if (inst->rep != REP_NONE)
  {
      if ((flags & F_REPABLE) == 0)
        inst->rep = REP_NONE;
  }

  inst->modrm = !(flags & F_NOMOD);
  assert (modrm == !(flags & F_NOMOD));

  /* decode addressing mode */
  sib = FALSE;
  base = index = -1;
  scale = 0;
  ndisp = 0; disp = 0;
  modbyte = *pinst;

  if (modrm && MODRM_MOD(modbyte) != 3)
  {
    if (inst->mode & MODE_ADDR32)
    {
      /* 32-bit addressing mode */
      switch (MODRM_MOD(modbyte))
      {
        case 0x00:
          if (MODRM_RM(modbyte) == 4)
          {
            /* sib byte included... */
            sib = TRUE;
          }
          else if (MODRM_RM(modbyte) == 5)
          {
            /* 32-bit direct addressing */
            ndisp = 4;
          }
          else
          {
            /* register indirect addressing */
            base = MODRM_RM(modbyte);
          }
          break;

        case 0x01:
          if (MODRM_RM(modbyte) == 4)
          {
            /* sib byte included... */
            sib = TRUE;
            ndisp = 1;
          }
          else
          {
            /* 8-bit displaced addressing */
            base = MODRM_RM(modbyte);
            ndisp = 1;
          }
          break;

        case 0x02:
          if (MODRM_RM(modbyte) == 4)
          {
            /* sib byte included... */
            sib = TRUE;
            ndisp = 4;
          }
          else
          {
            /* 32-bit displaced addressing */
            base = MODRM_RM(modbyte);
            ndisp = 4;
          }
          break;

        case 0x03:
          fatal("not an addressing mode");
      }

      if (sib)
      {
        byte_t sib = *++pinst;

        /* continue sib byte decode */
        if (SIB_BASE(sib) == 5 && MODRM_MOD(modbyte) == 0)
        {
          if (SIB_INDEX(sib) != 4)
            index = SIB_INDEX(sib);
          scale = SIB_SCALE(sib);
          ndisp = 4;
        }
        else
        {
          base = SIB_BASE(sib);
          if (SIB_INDEX(sib) != 4)
            index = SIB_INDEX(sib);
          scale = SIB_SCALE(sib);
        }
      }
    }
    else
    {
      /* 16-bit addressing mode */
      switch (MODRM_MOD(modbyte))
      {
        case 0x00:
          if (MODRM_RM(modbyte) == 6)
          {
            /* 16-bit direct addressing */
            ndisp = 2;
            goto done;
          }
          break;

        case 0x01:
          /* 8-bit displacement */
          ndisp = 1;
          break;

        case 0x02:
          /* 16-bit displacement */
          ndisp = 2;
          break;

        case 0x03:
          fatal("not an addressing mode");
      }

      /* determine base and index registers */
      switch (MODRM_RM(modbyte))
      {
        case 0x00:	base = MD_REG_BX; index = MD_REG_SI; break;
        case 0x01:	base = MD_REG_BX; index = MD_REG_DI; break;
        case 0x02:	base = MD_REG_BP; index = MD_REG_SI; break;
        case 0x03:	base = MD_REG_BP; index = MD_REG_DI; break;
        case 0x04:	index = MD_REG_SI; break;
        case 0x05:	index = MD_REG_DI; break;
        case 0x06:	base = MD_REG_BP; break;
        case 0x07:	base = MD_REG_BX; break;
      }
done:
      ;
    }

    /* extract (possible) displacement, all disps are sign-extended */
    {
      int dispidx = npfx + nopc + modrm + sib;

      if (inst->mode & MODE_ADDR32)
      {
        switch (ndisp)
        {
          case 0:
            disp = 0;
            break;
          case 1:
            disp = (sdword_t)(*(sbyte_t *)(inst->code + dispidx));
            break;
          case 4:
            disp = *(sdword_t *)(inst->code + dispidx);
            break;
          case 2: default:
            fatal("boom: invalid addressing mode");
        }
      }
      else /* 16-bit addressing mode */
      {
        switch (ndisp)
        {
          case 0:
            disp = 0;
            break;
          case 1:
            disp = (dword_t)(sword_t)(*(sbyte_t *)(inst->code + dispidx));
            break;
          case 2:
            disp = (dword_t)(*(sword_t *)(inst->code + dispidx));
            break;
          case 4: default:
            fatal("boom: invalid addressing mode");
        }
      }
    }
  }
  inst->sib = sib;
  inst->base = base;
  inst->index = index;
  inst->scale = scale;
  inst->ndisp = ndisp;
  inst->disp = disp;

  /* decode (possible) immediate value */
  nimm = imm = 0;
  if (flags & F_HASIMM)
  {
    int immidx = npfx + nopc + modrm + sib + ndisp;

    nimm = F_IMMSZ(inst->mode, flags);
    if (flags & F_UIMM)
    {
      /* unsigned immediate */
      switch (nimm)
      {
        case 0: imm = 0; break;
        case 1: imm = (dword_t)(*(byte_t *)(inst->code + immidx)); break;
        case 2: imm = (dword_t)(*(word_t *)(inst->code + immidx)); break;
        case 4: imm = *(dword_t *)(inst->code + immidx); break;
        default: fatal("boom");
      }
    }
    else
    {
      /* signed immediate */
      switch (nimm)
      {
        case 0: imm = 0; break;
        case 1: imm = (sdword_t)(*(sbyte_t *)(inst->code + immidx)); break;
        case 2: imm = (sdword_t)(*(sword_t *)(inst->code + immidx)); break;
        case 4: imm = *(sdword_t *)(inst->code + immidx); break;
        default: fatal("boom");
      }
    }
  }
  inst->nimm = nimm;
  inst->imm = imm;

  /* compute total instruction length */
  inst->len = npfx + nopc + modrm + sib + ndisp + nimm;

  /* done, return the enumerator for this instruction */
  return op;
}

/* enum md_opcode -> opcode flags, used by simulators */
const unsigned int md_op2flags[OP_MAX] = {
  NA, /* NA */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) CLASS, 
#define DEFUOP(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) CLASS,
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT) NA,
#define CONNECT(OP)
#include "machine.def"
};


  static unsigned long
md_set_decoder(const char *name,
    unsigned long mskbits, unsigned long offset,
    enum md_opcode op, unsigned long max_offset)
{
  unsigned long msk_base = mskbits & 0xff;
  unsigned long msk_bound = (mskbits >> 8) & 0xff;
  unsigned long msk;

  if(op == FCOM_Mt)
  {
    /* seems like msk_bound is only computing up to 2 */
    msk_bound = 3; //GL: Hack!
  }

  msk = msk_base;
  do {
    if ((msk + offset) >= MD_MAX_MASK)
      fatal("MASK_MAX is too small, inst=`%s', index=%d",
          name, msk + offset);
#ifdef DEBUG
    if (debugging && md_mask2op[msk + offset])
      warn("doubly defined opcode, inst=`%s', index=%d",
          name, msk + offset);
#endif

    md_mask2op[msk + offset] = op;
    msk++;
  } while (msk <= msk_bound);

  return MAX(max_offset, (msk-1) + offset);
}


/* intialize the inst decoder, this function builds the ISA decode tables */
  void
md_init_decoder(void)
{
  unsigned long max_offset = 0;
  unsigned long next_offset = 256;
  unsigned long offset = 0;

#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) \
  max_offset = md_set_decoder(NAME, (MSK), offset, (OP), max_offset);
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
  max_offset = md_set_decoder(#OP, (MSK), offset, (OP), max_offset);
#ifdef DEBUG
#define MD_INIT_DECODER_CHECK(OP)   if (debugging && md_opoffset[OP])					\
                                      warn("doubly defined opoffset, inst=`%s', op=%d, offset=%d",	\
                                           #OP, (int)(OP), offset)
#else
#define MD_INIT_DECODER_CHECK(OP) (void)0
#endif

#define CONNECT(OP)							\
  if ((max_offset+1) > next_offset)					\
  fatal("next offset too small");					\
  offset = next_offset;						\
  MD_INIT_DECODER_CHECK(OP);           \
  md_opoffset[OP] = offset;						\
  next_offset = offset + md_opmask[OP] + 1;				\
  if ((md_opmask[OP] & (md_opmask[OP]+1)) != 0)			\
  fatal("offset mask is not a power of two 0 1");


#include "machine.def"

#undef MD_INIT_DECODER_CHECK

  if (next_offset >= MD_MAX_MASK)
    fatal("MASK_MAX is too small, index==%d", max_offset);
#ifdef DEBUG
  if (debugging)
    info("max offset = %d...", max_offset);
#endif
}


/*
 * microcode field routines
 */
/* XXX */
word_t
md_uop_opc(const enum md_opcode uopcode)
{
  if (((unsigned int)uopcode) >= (1 << 14))
    fatal("UOP opcode field overflow: %d", (unsigned int)uopcode);
  return (word_t)uopcode;
}

byte_t
md_uop_reg(const enum md_xfield_t xval, const struct Mop_t * Mop, bool * bogus)
{
  byte_t res;

  switch (xval)
  {
    case XR_AL:		res = MD_REG_AL; break;
    case XR_AH:		res = MD_REG_AH; break;
    case XR_AX:		res = MD_REG_AX; break;
    case XR_EAX:	res = MD_REG_EAX; break;
    case XR_eAX:	res = MD_REG_eAX; break;
    case XR_CL:		res = MD_REG_CL; break;
    case XR_CH:		res = MD_REG_CH; break;
    case XR_CX:		res = MD_REG_CX; break;
    case XR_ECX:	res = MD_REG_ECX; break;
    case XR_eCX:	res = MD_REG_eCX; break;
    case XR_DL:		res = MD_REG_DL; break;
    case XR_DH:		res = MD_REG_DH; break;
    case XR_DX:		res = MD_REG_DX; break;
    case XR_EDX:	res = MD_REG_EDX; break;
    case XR_eDX:	res = MD_REG_eDX; break;
    case XR_BL:		res = MD_REG_BL; break;
    case XR_BH:		res = MD_REG_BH; break;
    case XR_BX:		res = MD_REG_BX; break;
    case XR_EBX:	res = MD_REG_EBX; break;
    case XR_eBX:	res = MD_REG_eBX; break;
    case XR_SP:		res = MD_REG_SP; break;
    case XR_ESP:	res = MD_REG_ESP; break;
    case XR_eSP:	res = MD_REG_eSP; break;
    case XR_BP:		res = MD_REG_BP; break;
    case XR_EBP:	res = MD_REG_EBP; break;
    case XR_eBP:	res = MD_REG_eBP; break;
    case XR_SI:		res = MD_REG_SI; break;
    case XR_ESI:	res = MD_REG_ESI; break;
    case XR_eSI:	res = MD_REG_eSI; break;
    case XR_DI:		res = MD_REG_DI; break;
    case XR_EDI:	res = MD_REG_EDI; break;
    case XR_eDI:	res = MD_REG_eDI; break;

    case XR_TMP0:	res = MD_REG_TMP0; break;
    case XR_TMP1:	res = MD_REG_TMP1; break;
    case XR_TMP2:	res = MD_REG_TMP2; break;
    case XR_ZERO:	res = MD_REG_ZERO; break;

    /* This is an ugly hack; it causes us to return a negative number that
       when passed to DSEG will map it to the uarch hardwired-zero register */
    case XR_SEGNONE: res = _DSEG(MD_REG_ZERO);

    case XR_ST0:	res = MD_REG_ST0; break;
    case XR_ST1:	res = MD_REG_ST1; break;
    case XR_ST2:	res = MD_REG_ST2; break;
    case XR_ST3:	res = MD_REG_ST3; break;
    case XR_ST4:	res = MD_REG_ST4; break;
    case XR_ST5:	res = MD_REG_ST5; break;
    case XR_ST6:	res = MD_REG_ST6; break;
    case XR_ST7:	res = MD_REG_ST7; break;

    case XR_FTMP0:	res = MD_REG_FTMP0; break;
    case XR_FTMP1:	res = MD_REG_FTMP1; break;
    case XR_FTMP2:	res = MD_REG_FTMP2; break;

    case XR_XMM0:   res = MD_REG_XMM0; break;
    case XR_XMM1:   res = MD_REG_XMM1; break;
    case XR_XMM2:   res = MD_REG_XMM2; break;
    case XR_XMM3:   res = MD_REG_XMM3; break;
    case XR_XMM4:   res = MD_REG_XMM4; break;
    case XR_XMM5:   res = MD_REG_XMM5; break;
    case XR_XMM6:   res = MD_REG_XMM6; break;
    case XR_XMM7:   res = MD_REG_XMM7; break;

    case XR_XMMTMP0:   res = MD_REG_XMMTMP0; break;
    case XR_XMMTMP1:   res = MD_REG_XMMTMP1; break;
    case XR_XMMTMP2:   res = MD_REG_XMMTMP2; break;

    case XF_RO:		res = RO; break;
    case XF_R:		res = R; break;
    case XF_RM:		res = RM; break;
    case XF_BASE:	res = (Mop->fetch.inst.base >= 0) ? Mop->fetch.inst.base : MD_REG_ZERO;
                  break;
    case XF_SEG:	res = (Mop->fetch.inst.seg != SEG_DEF) ? Mop->fetch.inst.seg-1 : (byte_t)SEG_INV;
                  /*fprintf(stderr, "md_uop_reg: %x\n", res);*/
                  break;
    case XF_INDEX:	res = (Mop->fetch.inst.index >= 0) ? Mop->fetch.inst.index : MD_REG_ZERO;
                    break;
    case XF_STI:	res = STI; break;

    case XF_CC:		res = CC; break;

    case XE_CCE:	res = CC_E; break;
    case XE_CCNE:	res = CC_NE; break;
    case XE_FCCB:	res = FCC_B; break;
    case XE_FCCNB:	res = FCC_NB; break;
    case XE_FCCE:	res = FCC_E; break;
    case XE_FCCNE:	res = FCC_NE; break;
    case XE_FCCBE:	res = FCC_BE; break;
    case XE_FCCNBE:	res = FCC_NBE; break;
    case XE_FCCU:	res = FCC_U; break;
    case XE_FCCNU:	res = FCC_NU; break;
    case XE_FNOP:	res = fpstk_nop; break;
    case XE_FPUSH:	res = fpstk_push; break;
    case XE_FPOP:	res = fpstk_pop; break;
    case XE_FPOPPOP:	res = fpstk_poppop; break;
    case XE_FP1:	res = 0; break;

    case XE_ILEN:	res = Mop->fetch.inst.len; break;

    default: if(bogus)
             {
               *bogus = TRUE;
               res = 0;
             }
             else
               fatal("bogus xfield register specifier: %d", (int)xval);
  }

  if (res > 15)
    fatal("register field overflow: %d", (int)xval);
  return res;
}

byte_t
md_uop_immb(const enum md_xfield_t xval, const struct Mop_t * Mop, bool * bogus)
{
  sdword_t res;

  switch (xval)
  {
    case XF_SYSCALL:
      res = 0x80;
      break;

    case XF_DISP:
      res = Mop->fetch.inst.disp;
      break;

    case XF_IMMB:
      res = Mop->fetch.inst.imm;
      break;

    case XE_ILEN:
      res = Mop->fetch.inst.len;
      break;

    case XE_ZERO:
      res = 0;
      break;

    case XE_ONE:
      res = 1;
      break;

    case XE_MONE:
      res = -1;
      break;

    case XE_SIZEV:
      res = ((Mop->fetch.inst.mode & MODE_OPER32) ? 4 : 2);
      break;

    case XE_MSIZEV:
      res = ((Mop->fetch.inst.mode & MODE_OPER32) ? -4 : -2);
      break;

    case XE_CF:		res = CF; break;
    case XE_DF:		res = DF; break;
    case XE_SFZFAFPFCF:	res = (SF|ZF|AF|PF|CF); break;

    default: if(bogus)
             {
               *bogus = TRUE;
               res = 0;
             }
             else
               fatal("bogus xfield immediate specifier: %d", (int)xval);
  }

  return res;
}

inline dword_t
md_uop_immv(const enum md_xfield_t xval, const struct Mop_t * Mop, bool * bogus)
{
  dword_t res;

  switch (xval)
  {
    case XF_DISP:
      res = Mop->fetch.inst.disp;
      break;

    case XF_IMMB:
    case XF_IMMV:
    case XF_IMMA:
      res = Mop->fetch.inst.imm;
      break;

    case XE_ILEN:
      res = Mop->fetch.inst.len;
      break;

    case XE_SIZEV_IMMW:
      res = ((Mop->fetch.inst.mode & MODE_OPER32) ? 4 : 2) + Mop->fetch.inst.imm;
      break;

    case XE_ZERO:
      res = 0;
      break;

    case XE_ONE:
      res = 1;
      break;

    case XE_MONE:
      res = (dword_t)-1;
      break;

    case XE_SIZEV:
      res = ((Mop->fetch.inst.mode & MODE_OPER32) ? 4 : 2);
      break;

    case XE_MSIZEV:
      res = ((Mop->fetch.inst.mode & MODE_OPER32) ? -4 : -2);
      break;

    case XE_CF:		res = CF; break;
    case XE_DF:		res = DF; break;
    case XE_SFZFAFPFCF:	res = (SF|ZF|AF|PF|CF); break;

    default: if(bogus)
             {
               *bogus = TRUE;
               res = 0;
             }
             else
               fatal("bogus xfield immediate (variable) specifier: %d", (int)xval);
  }
  return res;
}

inline dword_t
md_uop_lit(const enum md_xfield_t xval, const struct Mop_t * Mop, bool * bogus)
{
  dword_t res;

  switch (xval)
  {
    case XE_ZERO:
      res = 0;
      break;

    case XE_ONE:
      res = 1;
      break;

    case XE_MONE:
      res = (dword_t)-1;
      break;

    case XE_SIZEV:
      res = ((Mop->fetch.inst.mode & MODE_OPER32) ? 4 : 2);
      break;

    case XE_MSIZEV:
      res = ((Mop->fetch.inst.mode & MODE_OPER32) ? -4 : -2);
      break;

      //<<<<<<< x86.c
      //case XF_CC:         res = _CC;break;
      //=======
    case XF_CC: 	res = CC; break; // skumar

                  //>>>>>>> 1.3
    case XF_SCALE:	res = Mop->fetch.inst.scale; break;
    case XE_CCE:	res = CC_E; break;
    case XE_CCNE:	res = CC_NE; break;
    case XE_FCCB:	res = FCC_B; break;
    case XE_FCCNB:	res = FCC_NB; break;
    case XE_FCCE:	res = FCC_E; break;
    case XE_FCCNE:	res = FCC_NE; break;
    case XE_FCCBE:	res = FCC_BE; break;
    case XE_FCCNBE:	res = FCC_NBE; break;
    case XE_FCCU:	res = FCC_U; break;
    case XE_FCCNU:	res = FCC_NU; break;
    case XE_FNOP:	res = fpstk_nop; break;
    case XE_FPUSH:	res = fpstk_push; break;
    case XE_FPOP:	res = fpstk_pop; break;
    case XE_FPOPPOP:	res = fpstk_poppop; break;

    default: if(bogus)
             {
               *bogus = TRUE;
               res = 0;
             }
             else
               fatal("bogus literal specifier: %d", (int)xval);
  }
  return res;
}

/* UOP code selectors */
#define VMODE32 (Mop->fetch.inst.mode & MODE_OPER32)

#define XV(OP)								\
  ((Mop->fetch.inst.mode & MODE_OPER32) ? SYMCAT(OP,D) : SYMCAT(OP,W))
#define XVI(OP)								\
  ((Mop->fetch.inst.mode & MODE_OPER32) ? SYMCAT(OP,DI) : SYMCAT(OP,WI))

#define XA(OP)								\
  ((Mop->fetch.inst.mode & MODE_ADDR32) ? SYMCAT(OP,D) : SYMCAT(OP,W))
#define XAI(OP)								\
  ((Mop->fetch.inst.mode & MODE_ADDR32) ? SYMCAT(OP,DI) : SYMCAT(OP,WI))
/* concat OP with M and X, where M is D or W depending on the ADDR32 mask, X is size qualifier */
#define XAx(OP,X)								\
  ((Mop->fetch.inst.mode & MODE_ADDR32) ? SYMCAT3(OP,D,X) : SYMCAT3(OP,W,X))

#define XS(OP)								\
  ((Mop->fetch.inst.mode & MODE_STACK32) ? SYMCAT(OP,D) : SYMCAT(OP,W))
#define XSI(OP)								\
  ((Mop->fetch.inst.mode & MODE_STACK32) ? SYMCAT(OP,DI) : SYMCAT(OP,WI))

#define XSXV(OP)								\
  ( (Mop->fetch.inst.mode & MODE_STACK32) ?            \
      ( (Mop->fetch.inst.mode & MODE_OPER32) ? SYMCAT(OP,DD) : SYMCAT(OP,DW)) :            \
      ( (Mop->fetch.inst.mode & MODE_OPER32) ? SYMCAT(OP,WD) : SYMCAT(OP,WW))           \
  )
#define XAXV(OP)								\
  ( (Mop->fetch.inst.mode & MODE_ADDR32) ?            \
      ( (Mop->fetch.inst.mode & MODE_OPER32) ? SYMCAT(OP,DD) : SYMCAT(OP,DW)) :            \
      ( (Mop->fetch.inst.mode & MODE_OPER32) ? SYMCAT(OP,WD) : SYMCAT(OP,WW))           \
  )

/* microcode constructors */
/* FM = fusion mode (fuse to previous uop if user specifies this fusion flag) see machine.h */

#define UOP_R0(UV,OPC,FM)							\
  (flow[i++] = (  ((FM) << 32) \
                | (!!(UV) << 30)			\
                | (md_uop_opc(OPC) << 16)))
#define UOP_R1(UV,OPC,RD,FM)						\
  (flow[i++] = (  ((FM) << 32) \
                | (!!(UV) << 30)						\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)))
#define UOP_R2(UV,OPC,RD,RS,FM)						\
  (flow[i++] = (  ((FM) << 32) \
                | (!!(UV) << 30)						\
                | (md_uop_opc(OPC) << 16)			\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RS,Mop,bogus) << 8)))
#define UOP_R3(UV,OPC,RD,RS,RT,FM)						\
  (flow[i++] = (  ((FM) << 32) \
                | (!!(UV) << 30)						\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RS,Mop,bogus) << 8)				\
                | (md_uop_reg(RT,Mop,bogus) << 4)))
#define UOP_R4(UV,OPC,RD,RS,RT,RU,FM)					\
  (flow[i++] = (  ((FM) << 32) \
                | (!!(UV) << 30)						\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RS,Mop,bogus) << 8)				\
                | (md_uop_reg(RT,Mop,bogus) << 4)				\
                | md_uop_reg(RU,Mop,bogus)))
#define UOP_RL(UV,OPC,RD,RS,RT,LIT,FM)					\
  (flow[i++] = (  ((FM) << 32) \
                | (!!(UV) << 30)						\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RS,Mop,bogus) << 8)				\
                | (md_uop_reg(RT,Mop,bogus) << 4)				\
                | (md_uop_lit(LIT,Mop,bogus))))
#define UOP_RLI(UV,OPC,RD,RB,RI,SC,IV,FM)					\
  (flow[i++] = (  ((FM) << 32) \
                | (1ULL << 31)						\
                | (!!(UV) << 30)					\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RB,Mop,bogus) << 8)				\
                | (md_uop_reg(RI,Mop,bogus) << 4)				\
                | (md_uop_lit(SC,Mop,bogus))),				\
   flow[i++] = md_uop_immv(IV,Mop,bogus)),                                  \
   flow[i++] = 0
// Address computation with segment override - UCSD
#define UOP_RLI_OV(UV,OPC,RD,SG,RB,RI,SC,IV,FM)				\
  (flow[i++] = (  ((FM) << 32) \
                | (1ULL << 31)						\
                | (!!(UV) << 30)					\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RB,Mop,bogus) << 8)				\
                | (md_uop_reg(RI,Mop,bogus) << 4)				\
                | (md_uop_lit(SC,Mop,bogus))),				\
   flow[i++] = md_uop_immv(IV,Mop,bogus)),                                  \
   flow[i++] = md_uop_reg(SG,Mop,bogus)
#define UOP_IB(UV,OPC,RD,RS,IB,FM)						\
  (flow[i++] = (  ((FM) << 32) \
                | (!!(UV) << 30)						\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RS,Mop,bogus) << 8)				\
                | (md_uop_immb(IB,Mop,bogus))))
#define UOP_IV(UV,OPC,RD,RS,IV,FM)						\
  (flow[i++] = (  ((FM) << 32) \
                | (1ULL << 31)						\
                | (!!(UV) << 30)					\
                | (md_uop_opc(OPC) << 16)				\
                | (md_uop_reg(RD,Mop,bogus) << 12)				\
                | (md_uop_reg(RS,Mop,bogus) << 8)),				\
   flow[i++] = md_uop_immv(IV,Mop,bogus)),                                  \
   flow[i++] = 0

// Note FP-stack modifications
#define FP_STACK_OP(op) \
  Mop->decode.fpstack_op = (op)

/* UOP flow generator, returns a small non-cyclic program implementing OP,
   returns length of flow returned */
int
md_get_flow(struct Mop_t * Mop, uop_inst_t flow[MD_MAX_FLOWLEN], bool * bogus)
{
  int i = 0;

  switch (Mop->decode.op)
  {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,I1,I2,I3,OFLAGS,IFLAGS) \
    case OP:								\
                            OP##_FLOW;                                                        \
    break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
    case OP:							\
                          fatal("attempted to decode a linking opcode");
#define CONNECT(OP)
#include "machine.def"
    default: if(bogus)
             {
               *bogus = TRUE;
               i = 0;
             }
             else
               fatal("bogus opcode");
  }
  return i;
}
