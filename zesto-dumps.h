#ifndef ZESTO_DUMPS_H
#define ZESTO_DUMPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zesto-structs.h"

void dump_uop_alloc(struct uop_t * uop);
void dump_uop_timing(struct uop_t * uop);

#ifdef __cplusplus
}
#endif


#endif /*ZESTO_DUMPS_H*/
