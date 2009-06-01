/* libexo.h - EXO library interfaces (NEVER write another scanf()!) */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2002 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
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
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2002 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */

/*
 * EXO(-skeletal) definitions:
 *
 *   The EXO format is used to store and retrieve data structures from text
 *   files.  The following BNF definition defines the contents of an EXO file:
 *
 *	<exo_file>	:= <exo_term>
 *
 *	<exo_term>	:= <exo_term_list>
 *			   | INTEGER
 *			   | FLOAT
 *			   | CHAR
 *			   | STRING
 *
 *	<exo_term_list>	:= (<exo_term_list> ',' <exo_term>)
 *			   | <exo_term>
 */

#ifndef EXO_H
#define EXO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../host.h"
#include "../misc.h"
#include "../machine.h"

/* EXO file format versions */
#define EXO_FMT_MAJOR		1
#define EXO_FMT_MINOR		0

/* EXO term classes, keep this in sync with EXO_CLASS_STR */
enum exo_class_t {
  ec_integer,			/* EXO int value */
  ec_address,			/* EXO address value */
  ec_float,			/* EXO FP value */
  ec_char,			/* EXO character value */
  ec_string,			/* EXO string value */
  ec_list,			/* EXO list */
  ec_array,			/* EXO array */
  ec_token,			/* EXO token value */
  ec_blob,			/* EXO blob (Binary Large OBject) */
  ec_null,			/* used internally */
  ec_NUM
};

/* EXO term classes print strings */
extern char *exo_class_str[ec_NUM];

/* EXO token table entry */
struct exo_token_t {
  struct exo_token_t *next;	/* next element in a hash buck chain */
  char *str;			/* token string */
  int token;			/* token value */
};

struct exo_term_t {
  struct exo_term_t *next;	/* next element, when in a list */
  enum exo_class_t ec;		/* term node class */
  union {
    struct as_integer_t {
      exo_integer_t val;		/* integer value */
    } as_integer;
    struct as_address_t {
#if defined(sparc) && (defined(TARGET_ARM) || defined(TARGET_PISA))
      word_t _dummy;
#endif
      exo_address_t val;		/* address value */
    } as_address;
    struct as_float_t {
      exo_float_t val;			/* floating point value */
    } as_float;
    struct as_char_t {
      char val;				/* character value */
    } as_char;
    struct as_string_t {
      unsigned char *str;		/* string value */
    } as_string;
    struct as_list_t {
      struct exo_term_t *head;		/* list head pointer */
    } as_list;
    struct as_array_t {
      int size;				/* size of the array */
      struct exo_term_t **array;	/* list head pointer */
    } as_array;
    struct as_token_t {
      struct exo_token_t *ent;		/* token table entry */
    } as_token;
    struct as_blob_t {
      int size;				/* byte size of object */
      unsigned char *data;		/* pointer to blob data */
    } as_blob;
  } variant;
};
/* short-cut accessors */
#define as_integer	variant.as_integer
#define as_address	variant.as_address
#define as_float	variant.as_float
#define as_char		variant.as_char
#define as_string	variant.as_string
#define as_list		variant.as_list
#define as_array	variant.as_array
#define as_token	variant.as_token
#define as_blob		variant.as_blob

/* EXO array accessor, may be used as an L-value or R-value */
/* EXO array accessor, may be used as an L-value or R-value */
#define EXO_ARR(E,N)							\
  ((E)->ec != ec_array							\
   ? (fatal("not an array"), *(struct exo_term_t **)(NULL))		\
   : ((N) >= (E)->as_array.size						\
      ? (fatal("array bounds error"), *(struct exo_term_t **)(NULL))	\
      : (E)->as_array.array[(N)]))
#define SET_EXO_ARR(E,N,V)						\
  ((E)->ec != ec_array							\
   ? (void)fatal("not an array")					\
   : ((N) >= (E)->as_array.size						\
      ? (void)fatal("array bounds error")				\
      : (void)((E)->as_array.array[(N)] = (V))))

/* intern token TOKEN_STR */
struct exo_token_t *
exo_intern(char *token_str);		/* string to intern */

/* intern token TOKEN_STR as value TOKEN */
struct exo_token_t *
exo_intern_as(char *token_str,		/* string to intern */
	      int token);		/* internment value */

/*
 * create a new EXO term, usage:
 *
 *	exo_new(ec_integer, (exo_integer_t)<int>);
 *	exo_new(ec_address, (exo_address_t)<int>);
 *	exo_new(ec_float, (exo_float_t)<float>);
 *	exo_new(ec_char, (int)<char>);
 *      exo_new(ec_string, "<string>");
 *      exo_new(ec_list, <list_ent>..., NULL);
 *      exo_new(ec_array, <size>, <array_ent>..., NULL);
 *	exo_new(ec_token, "<token>");
 *	exo_new(ec_blob, <size>, <data_ptr>);
 */
struct exo_term_t *
exo_new(enum exo_class_t ec, ...);

/* release an EXO term */
void
exo_delete(struct exo_term_t *exo);

/* chain two EXO lists together, FORE is attached on the end of AFT */
struct exo_term_t *
exo_chain(struct exo_term_t *fore, struct exo_term_t *aft);

/* copy an EXO node */
struct exo_term_t *
exo_copy(struct exo_term_t *exo);

/* deep copy an EXO structure */
struct exo_term_t *
exo_deepcopy(struct exo_term_t *exo);

/* print an EXO term */
void
exo_print(struct exo_term_t *exo, FILE *stream);

/* read one EXO term from STREAM */
struct exo_term_t *
exo_read(FILE *stream);

/* check the format of an EXO component, returns non-zero if EXO matches,
   elements expected are specified by FMT as follows:

	i  - integer		a  - address
	f  - float		c  - character
	s  - string		() - list
	[] - array		t  - token
	b  - blob

   Node elements are written to trailing arguments which must be of type
   specified in the format type string FMT. (See eio.c for example usage.)

   NOTE1: lists () and arrays [] do NOT bind to a pointer argument.
   NOTE2: blobs 'b' bind to two arguments, the first is an 'int *' which
     specifies the size of the blob, and the second is an 'unsigned char **'
     which is updated with a pointer to the binary block (likely READ ONLY!).
*/
int exo_parse(struct exo_term_t *exo, char *fmt, ...);

/* just like exo_parse, but no argument variable binding, only checks */
int exo_check(struct exo_term_t *exo, char *fmt);


/* lexor components */
enum lex_t {
  lex_integer = 256,
  lex_address,
  lex_float,
  lex_char,
  lex_string,
  lex_token,
  lex_byte,
  lex_eof
};

#ifdef __cplusplus
}
#endif

#endif /* EXO_H */
