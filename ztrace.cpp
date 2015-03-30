#include "ztrace.h"

const char * ztrace_filename = nullptr;

#ifdef ZTRACE

#define MAX_TRACEBUFF_ITEMS 300000
static char tracebuff[MAX_CORES+1][MAX_TRACEBUFF_ITEMS][255];
static int tracebuff_head[MAX_CORES+1];
static int tracebuff_tail[MAX_CORES+1];
static int tracebuff_occupancy[MAX_CORES+1];

static FILE* ztrace_fp[MAX_CORES+1];

void ztrace_init(void)
{
  if(ztrace_filename && strcmp(ztrace_filename,""))
  {
    char buff[512];

    for (int i=0; i<num_cores; i++) {
      snprintf(buff, 512, "%s.%d", ztrace_filename, i);
      ztrace_fp[i] = fopen(buff,"w");
      if(!ztrace_fp[i])
        fatal("failed to open ztrace file %s", buff);
    }

    snprintf(buff, 512, "%s.uncore", ztrace_filename);
    ztrace_fp[num_cores] = fopen(buff,"w");
    if(!ztrace_fp[num_cores])
      fatal("failed to open ztrace file %s", buff);
  }
}

void trace(const int coreID, const char *fmt, ...)
{
  va_list v;
  va_start(v, fmt);

  vtrace(coreID, fmt, v);
}

void vtrace(const int coreID, const char *fmt, va_list v)
{
  int trace_id = (coreID == INVALID_CORE) ? num_cores : coreID;

  vsprintf(tracebuff[trace_id][tracebuff_tail[trace_id]], fmt, v);

  tracebuff_tail[trace_id] = modinc(tracebuff_tail[trace_id], MAX_TRACEBUFF_ITEMS);
  if(tracebuff_occupancy[trace_id] == MAX_TRACEBUFF_ITEMS)
    tracebuff_head[trace_id] = modinc(tracebuff_head[trace_id], MAX_TRACEBUFF_ITEMS);
  else
    tracebuff_occupancy[trace_id]++;
}

void ztrace_flush(void)
{
  for (int i=0; i < num_cores+1; i++) {
    if(tracebuff_occupancy[i] == 0)
      continue;

    FILE* fp = ztrace_fp[i];
    if(fp == NULL)
      continue;

    fprintf(fp, "==============================\n");
    fprintf(fp, "BEGIN TRACE (%d items)\n", tracebuff_occupancy[i]);

    int j = tracebuff_head[i];
    do
    {
      fprintf(fp, "%s", tracebuff[i][j]);
      j = modinc(j, MAX_TRACEBUFF_ITEMS);
    } while(j != tracebuff_tail[i]);

    fprintf(fp, "END TRACE\n");
    fprintf(fp, "==============================\n");
    fflush(fp);
    tracebuff_occupancy[i] = 0;
    tracebuff_head[i] = tracebuff_tail[i];
  }
}

void ztrace_Mop_ID(const struct Mop_t * Mop)
{
  if(Mop == NULL)
    return;

  int coreID = Mop->core->id;
  ZTRACE_PRINT(coreID, "%lld|M:%lld|",Mop->core->sim_cycle,Mop->oracle.seq);
  if(Mop->oracle.spec_mode)
    ZTRACE_PRINT(coreID, "X|");
  else
    ZTRACE_PRINT(coreID, ".|");
}

void ztrace_uop_ID(const struct uop_t * uop)
{
  if(uop == NULL)
    return;

  int coreID = uop->core->id;
  ZTRACE_PRINT(coreID, "%lld|u:%lld:%lld|", uop->core->sim_cycle,
          uop->decode.Mop_seq, (uop->decode.Mop_seq << UOP_SEQ_SHIFT) + uop->flow_index);
  if(uop->Mop && uop->Mop->oracle.spec_mode)
    ZTRACE_PRINT(coreID, "X|");
  else
    ZTRACE_PRINT(coreID, ".|");
}

void ztrace_uop_alloc(const struct uop_t * uop)
{
  if(uop == NULL)
    return;

  int coreID = uop->core->id;
  ZTRACE_PRINT(coreID, "ROB:%d|LDQ:%d|STQ:%d|RS:%d|port:%d|",
    uop->alloc.ROB_index, uop->alloc.LDQ_index, uop->alloc.STQ_index,
    uop->alloc.RS_index, uop->alloc.port_assignment);
}

void ztrace_uop_timing(const struct uop_t * uop)
{
  if(uop == NULL)
    return;

  int coreID = uop->core->id;
  ZTRACE_PRINT(coreID, "wd: %lld|", uop->timing.when_decoded);
  ZTRACE_PRINT(coreID, "wa: %lld|", uop->timing.when_allocated);
  for (int i=0; i<MAX_IDEPS; i++)
    ZTRACE_PRINT(coreID, "wit%d: %lld|", i, uop->timing.when_itag_ready[i]);
  for (int i=0; i<MAX_IDEPS; i++)
    ZTRACE_PRINT(coreID, "wiv%d: %lld|", i, uop->timing.when_ival_ready[i]);
  ZTRACE_PRINT(coreID, "wot: %lld|", uop->timing.when_otag_ready);
  ZTRACE_PRINT(coreID, "wr: %lld|", uop->timing.when_ready);
  ZTRACE_PRINT(coreID, "wi: %lld|", uop->timing.when_issued);
  ZTRACE_PRINT(coreID, "we: %lld|", uop->timing.when_exec);
  ZTRACE_PRINT(coreID, "wc: %lld|", uop->timing.when_completed);
}

/* called by oracle when Mop first executes */
void ztrace_print(const struct Mop_t * Mop)
{
  ztrace_Mop_ID(Mop);

  int coreID = Mop->core->id;

  // core id, PC{virtual,physical}
  ZTRACE_PRINT(coreID, "DEF|core=%d:virtPC=%x:physPC=%llx:op=",Mop->core->id,Mop->fetch.PC,v2p_translate(cores[coreID]->current_thread->asid,Mop->fetch.PC));
  // rep prefix and iteration
  if(Mop->fetch.inst.rep)
    ZTRACE_PRINT(coreID, "rep{%d}",Mop->decode.rep_seq);
  // opcode name
  ZTRACE_PRINT(coreID, "%s:",md_op2name[Mop->decode.op]);
  ZTRACE_PRINT(coreID, "trap=%d:",Mop->decode.is_trap);

  // ucode flow length
  ZTRACE_PRINT(coreID, "flow-length=%d\n",Mop->decode.flow_length);

  int i;
  int count=0;
  for(i=0;i<Mop->decode.flow_length;)
  {
    struct uop_t * uop = &Mop->uop[i];
    ztrace_uop_ID(uop);
    ZTRACE_PRINT(coreID, "DEF");
    if(uop->decode.BOM && !uop->decode.EOM) ZTRACE_PRINT(coreID, "-BOM");
    if(uop->decode.EOM && !uop->decode.BOM) ZTRACE_PRINT(coreID, "-EOM");
    // core id, uop number within flow
    ZTRACE_PRINT(coreID, "|core=%d:uop-number=%d:",Mop->core->id,count);
    // opcode name
    ZTRACE_PRINT(coreID, "op=%s",md_op2name[uop->decode.op]);
    if(uop->decode.in_fusion)
    {
      ZTRACE_PRINT(coreID, "-f");
      if(uop->decode.is_fusion_head)
        ZTRACE_PRINT(coreID, "H"); // fusion head
      else
        ZTRACE_PRINT(coreID, "b"); // fusion body
    }

    // register identifiers
    ZTRACE_PRINT(coreID, ":odep=%d:i0=%d:i1=%d:i2=%d:",
        uop->decode.odep_name,
        uop->decode.idep_name[0],
        uop->decode.idep_name[1],
        uop->decode.idep_name[2]);

    // load/store address and size
    if(uop->decode.is_load || uop->decode.is_sta)
      ZTRACE_PRINT(coreID, "VA=%lx:PA=%llx:mem-size=%d:fault=%d",(long unsigned int)uop->oracle.virt_addr,uop->oracle.phys_addr,uop->decode.mem_size,uop->oracle.fault);

    ZTRACE_PRINT(coreID, "\n");

    i += Mop->uop[i].decode.has_imm?3:1;
    count++;
  }
}

void ztrace_Mop_timing(const struct Mop_t * Mop)
{
  if (Mop == NULL)
    return;

  int coreID = Mop->core->id;
  ZTRACE_PRINT(coreID, "wfs: %lld|", Mop->timing.when_fetch_started);
  ZTRACE_PRINT(coreID, "wf: %lld|", Mop->timing.when_fetched);
  ZTRACE_PRINT(coreID, "wMS: %lld|", Mop->timing.when_MS_started);
  ZTRACE_PRINT(coreID, "wds: %lld|", Mop->timing.when_decode_started);
  ZTRACE_PRINT(coreID, "wd: %lld|", Mop->timing.when_decode_finished);
  ZTRACE_PRINT(coreID, "wcs: %lld|", Mop->timing.when_commit_started);
  ZTRACE_PRINT(coreID, "wc: %lld|", Mop->timing.when_commit_finished);
}

void ztrace_print(const struct Mop_t * Mop, const char * fmt, ... )
{
  if (Mop == NULL)
    return;

  va_list v;
  va_start(v, fmt);

  int coreID = Mop->core->id;

  ztrace_Mop_ID(Mop);
  ZTRACE_VPRINT(coreID, fmt, v);
  ZTRACE_PRINT(coreID, "\n");
}

void ztrace_print(const struct uop_t * uop, const char * fmt, ... )
{
  if (uop == NULL)
    return;

  va_list v;
  va_start(v, fmt);

  int coreID = uop->core->id;

  ztrace_uop_ID(uop);
  ZTRACE_VPRINT(coreID, fmt, v);
  ZTRACE_PRINT(coreID, "\n");
}

void ztrace_print(const int coreID, const char * fmt, ... )
{
  va_list v;
  va_start(v, fmt);

  ZTRACE_VPRINT(coreID, fmt, v);
  ZTRACE_PRINT(coreID, "\n");
}

void ztrace_print_start(const struct uop_t * uop, const char * fmt, ... )
{
  if (uop == NULL)
    return;

  va_list v;
  va_start(v, fmt);

  int coreID = uop->core->id;

  ztrace_uop_ID(uop);
  ZTRACE_VPRINT(coreID, fmt, v);
}

void ztrace_print_cont(const int coreID, const char * fmt, ... )
{
  va_list v;
  va_start(v, fmt);

  ZTRACE_VPRINT(coreID, fmt, v);
}

void ztrace_print_finish(const int coreID, const char * fmt, ... )
{
  va_list v;
  va_start(v, fmt);

  ZTRACE_VPRINT(coreID, fmt, v);
  ZTRACE_PRINT(coreID, "\n");
}

#endif
