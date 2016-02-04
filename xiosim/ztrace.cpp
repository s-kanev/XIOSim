#include <cstdarg>

#include "decode.h"
#include "memory.h"
#include "misc.h"
#include "sim.h"

#include "zesto-core.h"
#include "zesto-structs.h"

#include "ztrace.h"

#ifdef ZTRACE

#define MAX_TRACEBUFF_ITEMS 300000
static char tracebuff[MAX_CORES + 1][MAX_TRACEBUFF_ITEMS][255];
static int tracebuff_head[MAX_CORES + 1];
static int tracebuff_tail[MAX_CORES + 1];
static int tracebuff_occupancy[MAX_CORES + 1];

static FILE* ztrace_fp[MAX_CORES + 1];

void ztrace_init(void) {
    if (system_knobs.ztrace_filename && strcmp(system_knobs.ztrace_filename, "")) {
        char buff[512];

        for (int i = 0; i < num_cores; i++) {
            snprintf(buff, 512, "%s.%d", system_knobs.ztrace_filename, i);
            ztrace_fp[i] = fopen(buff, "w");
            if (!ztrace_fp[i])
                fatal("failed to open ztrace file %s", buff);
        }

        snprintf(buff, 512, "%s.uncore", system_knobs.ztrace_filename);
        ztrace_fp[num_cores] = fopen(buff, "w");
        if (!ztrace_fp[num_cores])
            fatal("failed to open ztrace file %s", buff);
    }
}

void trace(const int coreID, const char* fmt, ...) {
    va_list v;
    va_start(v, fmt);

    vtrace(coreID, fmt, v);
}

void vtrace(const int coreID, const char* fmt, va_list v) {
    int trace_id = (coreID == INVALID_CORE) ? num_cores : coreID;
    assert(trace_id >= 0 && trace_id <= num_cores);

    vsprintf(tracebuff[trace_id][tracebuff_tail[trace_id]], fmt, v);

    tracebuff_tail[trace_id] = modinc(tracebuff_tail[trace_id], MAX_TRACEBUFF_ITEMS);
    if (tracebuff_occupancy[trace_id] == MAX_TRACEBUFF_ITEMS)
        tracebuff_head[trace_id] = modinc(tracebuff_head[trace_id], MAX_TRACEBUFF_ITEMS);
    else
        tracebuff_occupancy[trace_id]++;
}

void ztrace_flush(void) {
    for (int i = 0; i < num_cores + 1; i++) {
        if (tracebuff_occupancy[i] == 0)
            continue;

        FILE* fp = ztrace_fp[i];
        if (fp == NULL)
            continue;

        fprintf(fp, "==============================\n");
        fprintf(fp, "BEGIN TRACE (%d items)\n", tracebuff_occupancy[i]);

        int j = tracebuff_head[i];
        do {
            fprintf(fp, "%s", tracebuff[i][j]);
            j = modinc(j, MAX_TRACEBUFF_ITEMS);
        } while (j != tracebuff_tail[i]);

        fprintf(fp, "END TRACE\n");
        fprintf(fp, "==============================\n");
        fflush(fp);
        tracebuff_occupancy[i] = 0;
        tracebuff_head[i] = tracebuff_tail[i];
    }
}

void ztrace_Mop_ID(const struct Mop_t* Mop) {
    if (Mop == NULL)
        return;

    int coreID = Mop->core->id;
    trace(coreID, "%" PRId64"|M:%" PRId64"|", Mop->core->sim_cycle, Mop->oracle.seq);
    if (Mop->oracle.spec_mode)
        trace(coreID, "X|");
    else
        trace(coreID, ".|");
}

void ztrace_uop_ID(const struct uop_t* uop) {
    if (uop == NULL)
        return;

    int coreID = uop->core->id;
    trace(coreID,
                 "%" PRId64"|u:%" PRId64":%" PRId64"|",
                 uop->core->sim_cycle,
                 uop->decode.Mop_seq,
                 uop->decode.uop_seq);
    if (uop->Mop && uop->Mop->oracle.spec_mode)
        trace(coreID, "X|");
    else
        trace(coreID, ".|");
}

void ztrace_uop_alloc(const struct uop_t* uop) {
    if (uop == NULL)
        return;

    int coreID = uop->core->id;
    trace(coreID,
                 "ROB:%d|LDQ:%d|STQ:%d|RS:%d|port:%d|",
                 uop->alloc.ROB_index,
                 uop->alloc.LDQ_index,
                 uop->alloc.STQ_index,
                 uop->alloc.RS_index,
                 uop->alloc.port_assignment);
}

void ztrace_uop_timing(const struct uop_t* uop) {
    if (uop == NULL)
        return;

    int coreID = uop->core->id;
    trace(coreID, "wd: %" PRId64"|", uop->timing.when_decoded);
    trace(coreID, "wa: %" PRId64"|", uop->timing.when_allocated);
    for (size_t i = 0; i < MAX_IDEPS; i++)
        trace(coreID, "wit%d: %" PRId64"|", (int)i, uop->timing.when_itag_ready[i]);
    for (size_t i = 0; i < MAX_IDEPS; i++)
        trace(coreID, "wiv%d: %" PRId64"|", (int)i, uop->timing.when_ival_ready[i]);
    trace(coreID, "wot: %" PRId64"|", uop->timing.when_otag_ready);
    trace(coreID, "wr: %" PRId64"|", uop->timing.when_ready);
    trace(coreID, "wi: %" PRId64"|", uop->timing.when_issued);
    trace(coreID, "we: %" PRId64"|", uop->timing.when_exec);
    trace(coreID, "wc: %" PRId64"|", uop->timing.when_completed);
}

/* called by oracle when Mop first executes */
void ztrace_print(const struct Mop_t* Mop) {
    ztrace_Mop_ID(Mop);

    int coreID = Mop->core->id;

    // core id, PC{virtual,physical}
    trace(coreID,
                 "DEF|core=%d:virtPC=%" PRIxPTR":physPC=%" PRIx64":op=%s:",
                 Mop->core->id,
                 Mop->fetch.PC,
                 xiosim::memory::v2p_translate(cores[coreID]->asid, Mop->fetch.PC),
                 xiosim::x86::print_Mop(Mop).c_str());
    // ucode flow length
    trace(coreID, "flow-length=%d\n", (int)Mop->decode.flow_length);

    int count = 0;
    for (size_t i = 0; i < Mop->decode.flow_length;) {
        struct uop_t* uop = &Mop->uop[i];
        ztrace_uop_ID(uop);
        trace(coreID, "DEF");
        if (uop->decode.BOM && !uop->decode.EOM)
            trace(coreID, "-BOM");
        if (uop->decode.EOM && !uop->decode.BOM)
            trace(coreID, "-EOM");
        // core id, uop number within flow
        trace(coreID, "|core=%d:uop-number=%d:", Mop->core->id, count);
        if (uop->decode.in_fusion) {
            trace(coreID, "f");
            if (uop->decode.is_fusion_head)
                trace(coreID, "H");  // fusion head
            else
                trace(coreID, "b");  // fusion body
        }

        // register identifiers
        trace(coreID,
                     ":odep0=%s:odep1=%s:i0=%s:i1=%s:i2=%s",
                     xed_reg_enum_t2str(uop->decode.odep_name[0]),
                     xed_reg_enum_t2str(uop->decode.odep_name[1]),
                     xed_reg_enum_t2str(uop->decode.idep_name[0]),
                     xed_reg_enum_t2str(uop->decode.idep_name[1]),
                     xed_reg_enum_t2str(uop->decode.idep_name[2]));

        // load/store address and size
        if (uop->decode.is_load || uop->decode.is_sta)
            trace(coreID,
                         ":VA=%" PRIxPTR":PA=%" PRIx64":mem-size=%d",
                         uop->oracle.virt_addr,
                         uop->oracle.phys_addr,
                         uop->decode.mem_size);

        trace(coreID, "\n");

        i += Mop->uop[i].decode.has_imm ? 3 : 1;
        count++;
    }
}

void ztrace_Mop_timing(const struct Mop_t* Mop) {
    if (Mop == NULL)
        return;

    int coreID = Mop->core->id;
    trace(coreID, "wfs: %" PRId64"|", Mop->timing.when_fetch_started);
    trace(coreID, "wf: %" PRId64"|", Mop->timing.when_fetched);
    trace(coreID, "wMS: %" PRId64"|", Mop->timing.when_MS_started);
    trace(coreID, "wds: %" PRId64"|", Mop->timing.when_decode_started);
    trace(coreID, "wd: %" PRId64"|", Mop->timing.when_decode_finished);
    trace(coreID, "wcs: %" PRId64"|", Mop->timing.when_commit_started);
    trace(coreID, "wc: %" PRId64"|", Mop->timing.when_commit_finished);
}

void ztrace_print(const struct Mop_t* Mop, const char* fmt, ...) {
    if (Mop == NULL)
        return;

    va_list v;
    va_start(v, fmt);

    int coreID = Mop->core->id;

    ztrace_Mop_ID(Mop);
    vtrace(coreID, fmt, v);
    trace(coreID, "\n");
}

void ztrace_print(const struct uop_t* uop, const char* fmt, ...) {
    if (uop == NULL)
        return;

    va_list v;
    va_start(v, fmt);

    int coreID = uop->core->id;

    ztrace_uop_ID(uop);
    vtrace(coreID, fmt, v);
    trace(coreID, "\n");
}

void ztrace_print(const int coreID, const char* fmt, ...) {
    va_list v;
    va_start(v, fmt);

    vtrace(coreID, fmt, v);
    trace(coreID, "\n");
}

void ztrace_print_start(const struct uop_t* uop, const char* fmt, ...) {
    if (uop == NULL)
        return;

    va_list v;
    va_start(v, fmt);

    int coreID = uop->core->id;

    ztrace_uop_ID(uop);
    vtrace(coreID, fmt, v);
}

void ztrace_print_cont(const int coreID, const char* fmt, ...) {
    va_list v;
    va_start(v, fmt);

    vtrace(coreID, fmt, v);
}

void ztrace_print_finish(const int coreID, const char* fmt, ...) {
    va_list v;
    va_start(v, fmt);

    vtrace(coreID, fmt, v);
    trace(coreID, "\n");
}

#endif
