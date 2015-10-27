/*----------------------------------------------------------*/
/* saturating two bit counter implemented with no branching */
/*----------------------------------------------------------*/
typedef unsigned char my2bc_t;

#define _2BC_TABLE 0xD4E8
#define MY2BC_DIR_MASK 0x01
#define MY2BC_HYST_MASK 0x02
#define MY2BC_DIR(ctr) ((ctr)&MY2BC_DIR_MASK)
#define MY2BC_HYST(ctr) ((ctr)&MY2BC_HYST_MASK)

#define MY2BC_UPDATE(ctr,dir) (ctr)=(_2BC_TABLE>>(((ctr<<1)+(dir))<<1))&3
/* _2BC_table is a bit packed version of the state transition table
   for the 2bC:

   The bits are switched in their places (i.e. bit 0 = direction bit
   and bit 1 = confidence/hysteresis bit).  This allows the extraction
   of the direction bit with a simple AND instead of a SHIFT+AND.

   prev
   2bC
   LD  inc  dec
      +--------
   00 | 10   00  (8)
   01 | 11   10  (E)
   10 | 01   00  (4)
   11 | 11   01  (D)

   (LD: 'L'east Signif. Bit of 2bc,
   'D'irection Bit)

   The previous counter value and the branch outcome direction are
   used to shift the packed table by the proper amount, and then the
   two bits are masked out.

 */

/* similar to MY2BC_UPDATE, but these macros inc/dec (saturating) if
   the predicate p is true.

      (COND_INC)          (COND_DEC)
   LD   p=1 p=0        LD   p=1 p=0
      +--------           +--------
   00 | 10  00  (8)    00 | 00  00  (0)
   01 | 11  01  (D)    01 | 10  01  (9)
   10 | 01  10  (6)    10 | 00  10  (2)
   11 | 11  11  (F)    11 | 01  11  (7)
 */
#define _2BC_CINC_TABLE 0xF6D8
#define _2BC_CDEC_TABLE 0x7290

#define MY2BC_COND_INC(ctr,p) (ctr)=(_2BC_CINC_TABLE>>(((ctr<<1)+(p))<<1))&3
#define MY2BC_COND_DEC(ctr,p) (ctr)=(_2BC_CDEC_TABLE>>(((ctr<<1)+(p))<<1))&3

#define MY2BC_STRONG_TAKEN (3)
#define MY2BC_WEAKLY_TAKEN (1)
#define MY2BC_WEAKLY_NT (2)
#define MY2BC_STRONG_NT (0)
