#ifdef __cplusplus
extern "C" {
#endif

#include "zesto-dumps.h"

void dump_uop_alloc(struct uop_t * uop)
{
   fprintf(stderr, "ROB_index: %d\n", uop->alloc.ROB_index);
   fprintf(stderr, "LDQ_index: %d\n", uop->alloc.LDQ_index);
   fprintf(stderr, "STQ_index: %d\n", uop->alloc.STQ_index);
   fprintf(stderr, "port: %d\n", uop->alloc.port_assignment); 
}

void dump_uop_timing(struct uop_t * uop)
{
   fprintf(stderr, "when_decoded: %lld\n", uop->timing.when_decoded);
   fprintf(stderr, "when_allocated: %lld\n", uop->timing.when_allocated);
   fprintf(stderr, "when_ivals_ready: ");
   for(int i = 0; i<MAX_IDEPS; i++)
    fprintf(stderr, "%lld ", uop->timing.when_ival_ready[i]);
   fprintf(stderr, "\n");

   fprintf(stderr, "when_otag_ready: %lld\n", uop->timing.when_otag_ready);
   fprintf(stderr, "when_ready: %lld\n", uop->timing.when_ready);
   fprintf(stderr, "when_issued: %lld\n", uop->timing.when_issued);
   fprintf(stderr, "when_exec: %lld\n", uop->timing.when_exec);
   fprintf(stderr, "when_completed: %lld\n", uop->timing.when_completed);
}


#ifdef __cplusplus
}
#endif


