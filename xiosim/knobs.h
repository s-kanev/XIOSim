#ifndef __KNOBS_H__
#define __KNOBS_H__

#include "fu.h"
#include "host.h"
#include "uop_cracker.h"

/* maximum number of sub-components in a hybrid branch predictor */
const int MAX_HYBRID_BPRED = 8;
const int MAX_DECODE_WIDTH = 16;
const int MAX_EXEC_WIDTH = 16;
/* per cache; so IL1 can have MAX_PREFETCHERS prefetchers independent of the DL1 or LLC */
const int MAX_PREFETCHERS = 4;

/* holds all of the parameters for a core, plus any additional helper variables
   for command line parsing (e.g., config strings) */
struct core_knobs_t {
    const char* model;
    double default_cpu_speed; /* in MHz */

    struct {
        int byteQ_size;
        int byteQ_linesize; /* in bytes */
        int depth;          /* predecode pipe */
        int width;          /* predecode pipe */
        int IQ_size;
        int jeclear_delay;

        int num_bpred_components;
        const char* bpred_opt_str[MAX_HYBRID_BPRED];
        const char* fusion_opt_str;
        const char* dirjmpbtb_opt_str;
        const char* indirjmpbtb_opt_str;
        const char* ras_opt_str;
    } fetch;

    struct {
        int depth;        /* decode pipe */
        int width;        /* decode pipe */
        int target_stage; /* stage in which a wrong taken BTB target is detected and corrected */
        int* max_uops;    /* maximum number of uops emittable per decoder */
        int MS_latency;   /* number of cycles from decoder[0] to uROM/MS */
        int uopQ_size;
        xiosim::x86::fusion_flags_t fusion_mode; /* which fusion types are allowed */
        int decoders[MAX_DECODE_WIDTH];
        int num_decoder_specs;
        int branch_decode_limit; /* maximum number of branches decoded per cycle */
    } decode;

    struct {
        int depth;        /* alloc pipe */
        int width;        /* alloc pipe */
        bool drain_flush; /* use drain flush? */
    } alloc;

    struct {
        int RS_size;
        int LDQ_size;
        int STQ_size;
        struct {
            int num_FUs;
            int* ports;
        } port_binding[xiosim::NUM_FU_CLASSES];
        int num_exec_ports;
        int payload_depth;
        int fu_bindings[xiosim::NUM_FU_CLASSES][MAX_EXEC_WIDTH];
        int num_bindings[xiosim::NUM_FU_CLASSES];
        int latency[xiosim::NUM_FU_CLASSES];
        int issue_rate[xiosim::NUM_FU_CLASSES];
        const char* memdep_opt_str;
        int fp_penalty; /* extra cycles to forward to FP cluster */
        bool tornado_breaker;
        bool throttle_partial;
        const char* repeater_opt_str;
    } exec;

    struct {
        /* prefetch arguments */
        int IL1_num_PF;
        int IL1_PFFsize;
        int IL1_PFthresh;
        int IL1_PFmax;
        int IL1_PF_buffer_size;
        int IL1_PF_filter_size;
        int IL1_PF_filter_reset;
        bool IL1_PF_on_miss;
        int IL1_WMinterval;
        double IL1_low_watermark;
        double IL1_high_watermark;
        float IL1_magic_hit_rate;
        int DL1_num_PF;
        int DL1_PFFsize;
        int DL1_PFthresh;
        int DL1_PFmax;
        int DL1_PF_buffer_size;
        int DL1_PF_filter_size;
        int DL1_PF_filter_reset;
        bool DL1_PF_on_miss;
        int DL1_WMinterval;
        double DL1_low_watermark;
        double DL1_high_watermark;
        const char* DL1_MSHR_cmd;
        float DL1_magic_hit_rate;
        int DL2_num_PF;
        int DL2_PFFsize;
        int DL2_PFthresh;
        int DL2_PFmax;
        int DL2_PF_buffer_size;
        int DL2_PF_filter_size;
        int DL2_PF_filter_reset;
        bool DL2_PF_on_miss;
        int DL2_WMinterval;
        double DL2_low_watermark;
        double DL2_high_watermark;
        const char* DL2_MSHR_cmd;
        float DL2_magic_hit_rate;

        /* for storing command line parameters */
        const char* IL1_opt_str;
        const char* ITLB_opt_str;
        const char* DL1_opt_str;
        const char* DL2_opt_str;
        const char* DTLB_opt_str;
        const char* DTLB2_opt_str;

        const char* IL1PF_opt_str[MAX_PREFETCHERS];
        const char* DL1PF_opt_str[MAX_PREFETCHERS];
        const char* DL2PF_opt_str[MAX_PREFETCHERS];

        const char* IL1_controller_opt_str;
        const char* ITLB_controller_opt_str;
        const char* DL1_controller_opt_str;
        const char* DL2_controller_opt_str;
        const char* DTLB_controller_opt_str;
        const char* DTLB2_controller_opt_str;

        bool DL1_rep_req;
    } memory;

    struct {
        int ROB_size;
        int width;
        int branch_limit; /* maximum number of branches committed per cycle */
        int pre_commit_depth;
    } commit;

};

struct uncore_knobs_t {
    const char* LLC_opt_str;
    const char* LLC_PF_opt_str[MAX_PREFETCHERS];
    const char* LLC_MSHR_cmd;
    const char* LLC_controller_str;

    float LLC_magic_hit_rate;
    int LLC_num_PF;
    int LLC_PFFsize;
    int LLC_PFthresh;
    int LLC_PFmax;
    int LLC_PF_buffer_size;
    int LLC_PF_filter_size;
    int LLC_PF_filter_reset;
    int LLC_WMinterval;
    bool LLC_PF_on_miss;
    double LLC_low_watermark;
    double LLC_high_watermark;
    double LLC_speed;

    int fsb_width;
    bool fsb_DDR;
    double fsb_speed;
    bool fsb_magic;

    const char* MC_opt_string;

    const char* dram_opt_string;
};

struct system_knobs_t {
    /* spin on assertion failure so we can attach a debbuger */
    bool assert_spin;
    int rand_seed;
    int num_cores;
    tick_t heartbeat_frequency;
    /* Prefix for ztrace output files.
     * Final filenames will be @ztrace_filename.{coreID}. */
    const char* ztrace_filename;
    /* Simulator output file. */
    const char* sim_simout;

    /* Power simulation knobs. */
    struct {
        bool compute;
        int rtp_interval;
        const char* rtp_filename;
    } power;

    md_addr_t stopwatch_start_pc;
    md_addr_t stopwatch_stop_pc;

    int scheduler_tick;

    /* Core allocation policy. Valid options:
     * "gang", "local", or "penalty". See pintool/base_allocator.h for more
     * details.
     */
    const char* allocator;

    /* Speedup model to use when calculating
     * core allocations. Valid options: "linear" or "log". See
     * pintool/base_speedup_model.h for more details.
     */
    const char* speedup_model;

    /* Optimization target when computing core
     * allocations. Valid options: "energy" or "throughput". See
     * pintool/base_speedup_model.h for more details.
     */
    const char* allocator_opt_target;

    const char* dvfs_opt_str;
    int dvfs_interval;
};
#endif /* __KNOBS_H__ */
