/* range.c - program execution range routines */
/*
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "symbol.h"
#include "loader.h"
#include "range.h"

/* parse execution position *PSTR to *POS */
const char *						/* error string, or NULL */
range_parse_pos(struct thread_t * core,
    char *pstr,			/* execution position string */
		struct range_pos_t *pos)	/* position return buffer */
{
  char *s, *endp;
  struct sym_sym_t *sym;
#if !defined(__CYGWIN32__)
  extern int errno;
#endif

  /* determine position type */
  if (pstr[0] == '@')
    {
      /* address position */
      pos->ptype = pt_addr;
      s = pstr + 1;
    }
  else if (pstr[0] == '#')
    {
      /* cycle count position */
      pos->ptype = pt_cycle;
      s = pstr + 1;
    }
  else
    {
      /* inst count position */
      pos->ptype = pt_inst;
      s = pstr;
    }

  /* get position value */
  errno = 0;
  pos->pos = (counter_t)strtoul(s, &endp, /* parse base */0);
  if (!errno && !*endp)
    {
      /* good conversion */
      return NULL;
    }

  /* else, not an integer, attempt double conversion */
  errno = 0;
  pos->pos = (counter_t)strtod(s, &endp);
  if (!errno && !*endp)
    {
      /* good conversion */
      /* FIXME: ignoring decimal point!! */
      return NULL;
    }

  /* else, attempt symbol lookup */
  sym_loadsyms(core->loader.prog_fname, /* !locals */FALSE);
  sym = sym_bind_name(s, NULL, sdb_any);
  if (sym != NULL)
    {
      pos->pos = (counter_t)sym->addr;
      return NULL;
    }

  /* else, no binding made */
  return "cannot bind execution position to a value";
}

/* print execution position *POS */
void
range_print_pos(struct range_pos_t *pos,	/* execution position */
		FILE *stream)			/* output stream */
{
  switch (pos->ptype)
    {
    case pt_addr:
      myfprintf(stream, "@0x%08p", (md_addr_t)pos->pos);
      break;
    case pt_inst:
      fprintf(stream, "%.0f", (double)pos->pos);
      break;
    case pt_cycle:
      fprintf(stream, "#%.0f", (double)pos->pos);
      break;
    default:
      panic("bogus execution position type");
    }
}

/* parse execution range *RSTR to *RANGE */
const char *						/* error string, or NULL */
range_parse_range(
      struct thread_t * core,
      char *rstr,			/* execution range string */
		  struct range_range_t *range)	/* range return buffer */
{
  char *pos1, *pos2, *p, buf[512];
  const char *errstr;

  /* make a copy of the execution range */
  strcpy(buf, rstr);
  pos1 = buf;

  /* find mid-point */
  p = buf;
  while (*p != ':' && *p != '\0')
    {
      p++;
    }
  if (*p != ':')
    return "badly formed execution range";
  *p = '\0';

  /* this is where the second position will start */
  pos2 = p + 1;

  /* parse start position */
  if (*pos1 && *pos1 != ':')
    {
      errstr = range_parse_pos(core,pos1, &range->start);
      if (errstr)
	return errstr;
    }
  else
    {
      /* default start range */
      range->start.ptype = pt_inst;
      range->start.pos = 0;
    }

  /* parse end position */
  if (*pos2)
    {
      if (*pos2 == '+')
	{
	  int delta;
	  char *endp;
#if !defined(__CYGWIN32__)
	  extern int errno;
#endif

	  /* get delta value */
	  errno = 0;
	  delta = strtol(pos2 + 1, &endp, /* parse base */0);
	  if (!errno && !*endp)
	    {
	      /* good conversion */
	      range->end.ptype = range->start.ptype;
	      range->end.pos = range->start.pos + delta;
	    }
	  else
	    {
	      /* bad conversion */
	      return "badly formed execution range delta";
	    }
	}
      else
	{
	  errstr = range_parse_pos(core,pos2, &range->end);
	  if (errstr)
	    return errstr;
	}
    }
  else
    {
      /* default end range */
      range->end.ptype = range->start.ptype;
#ifdef HOST_HAS_QWORD
      range->end.pos = ULL(0x7fffffffffffffff);
#else /* !__GNUC__ */
      range->end.pos = 281474976645120.0;
#endif /* __GNUC__ */
    }

  /* no error */
  return NULL;
}

/* print execution range *RANGE */
void
range_print_range(struct range_range_t *range,	/* execution range */
		  FILE *stream)			/* output stream */
{
  range_print_pos(&range->start, stream);
  fprintf(stream, ":");
  range_print_pos(&range->end, stream);
}

/* determine if inputs match execution position */
int						/* relation to position */
range_cmp_pos(struct range_pos_t *pos,		/* execution position */
	      counter_t val)			/* position value */
{
  if (val < pos->pos)
    return /* before */-1;
  else if (val == pos->pos)
    return /* equal */0;
  else /* if (pos->pos < val) */
    return /* after */1;
}

/* determine if inputs are in range */
int						/* relation to range */
range_cmp_range(struct range_range_t *range,	/* execution range */
		counter_t val)			/* position value */
{
  if (range->start.ptype != range->end.ptype)
    panic("invalid range");

  if (val < range->start.pos)
    return /* before */-1;
  else if (range->start.pos <= val && val <= range->end.pos)
    return /* inside */0;
  else /* if (range->end.pos < val) */
    return /* after */1;
}

/* determine if inputs are in range, passes all possible info needed */
int						/* relation to range */
range_cmp_range1(struct range_range_t *range,	/* execution range */
		 md_addr_t addr,		/* address value */
		 counter_t icount,		/* instruction count */
		 counter_t cycle)		/* cycle count */
{
  if (range->start.ptype != range->end.ptype)
    panic("invalid range");

  switch (range->start.ptype)
    {
    case pt_addr:
      if (addr < (md_addr_t)range->start.pos)
	return /* before */-1;
      else if ((md_addr_t)range->start.pos <= addr && addr <= (md_addr_t)range->end.pos)
	return /* inside */0;
      else /* if (range->end.pos < addr) */
	return /* after */1;
      break;
    case pt_inst:
      if (icount < range->start.pos)
	return /* before */-1;
      else if (range->start.pos <= icount && icount <= range->end.pos)
	return /* inside */0;
      else /* if (range->end.pos < icount) */
	return /* after */1;
      break;
    case pt_cycle:
      if (cycle < range->start.pos)
	return /* before */-1;
      else if (range->start.pos <= cycle && cycle <= range->end.pos)
	return /* inside */0;
      else /* if (range->end.pos < cycle) */
	return /* after */1;
      break;
    default:
      panic("bogus range type");
    }
}

#ifdef __cplusplus
}
#endif
