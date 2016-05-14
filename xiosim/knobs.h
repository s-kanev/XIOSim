#ifndef __KNOBS_H__
#define __KNOBS_H__

#include <string>
#include <vector>

#include "fu.h"
#include "host.h"
#include "uop_cracker.h"

/* maximum number of sub-components in a hybrid branch predictor */
const int MAX_HYBRID_BPRED = 8;
/* per cache; so IL1 can have MAX_PREFETCHERS prefetchers independent of the DL1 or LLC */
const int MAX_PREFETCHERS = 4;


struct prefetcher_knobs_t {
    /* prefetchers */
    int num_pf;
    const char* pf_opt_str[MAX_PREFETCHERS];

    /* prefetcher FIFO */
    int pff_size;

    /* prefetcher filter */
    int pf_thresh;
    int pf_max;
    int pf_buffer_size;
    int pf_filter_size;
    int pf_filter_reset;
    bool pf_on_miss;

    /* watermark */
    int watermark_interval;
    double low_watermark;
    double high_watermark;
};

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
        std::vector<int> max_uops; /* maximum number of uops emittable per decoder */
        int MS_latency;   /* number of cycles from decoder[0] to uROM/MS */
        int uopQ_size;
        xiosim::x86::fusion_flags_t fusion_mode; /* which fusion types are allowed */
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
            std::vector<int> ports;
        } port_binding[xiosim::NUM_FU_CLASSES];
        int num_exec_ports;
        int payload_depth;
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
        prefetcher_knobs_t IL1_pf;
        prefetcher_knobs_t DL1_pf;
        prefetcher_knobs_t DL2_pf;

        const char* DL1_MSHR_cmd;
        const char* DL2_MSHR_cmd;

        float IL1_magic_hit_rate;
        float DL1_magic_hit_rate;
        float DL2_magic_hit_rate;

        /* for storing command line parameters */
        const char* IL1_opt_str;
        const char* ITLB_opt_str;
        const char* DL1_opt_str;
        const char* DL2_opt_str;
        const char* DTLB_opt_str;
        const char* DTLB2_opt_str;

        const char* IL1_controller_opt_str;
        const char* ITLB_controller_opt_str;
        const char* DL1_controller_opt_str;
        const char* DL2_controller_opt_str;
        const char* DTLB_controller_opt_str;
        const char* DTLB2_controller_opt_str;

        /* Enable/disable sampling of cache misses. */
        bool IL1_sample_misses;
        bool DL1_sample_misses;
        bool DL2_sample_misses;

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
    const char* LLC_MSHR_cmd;
    float LLC_magic_hit_rate;
    bool LLC_sample_misses;
    double LLC_speed;

    const char* LLC_controller_str;

    prefetcher_knobs_t LLC_pf;

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

    /* Frequency at which to sample the PC of a cache miss. */
    unsigned long cache_miss_sample_parameter;

    /* Prefix for profiling result files.
     * Filenames are prefix.<core>.<profile_id>. */
    const char* profiling_file_prefix;
    /* Start points for profiling. A list of symbol(+offset).
     * For example, {"tc_malloc", "tc_free+0x5"} will start profiles at the entry point
     * point of tc_malloc, and 5 bytes below the entry point of tc_free. */
    std::vector<std::string> profiling_start;
    /* End points for profiling. Either empty, or a list of symbol(+offset), the same
     * length as profiling_start. If empty, profiles will stop at the exit points of
     * @symbol. */
    std::vector<std::string> profiling_stop;

    /* Function names to ignore. */
    std::vector<std::string> ignored_funcs;
    /* Individual instructions to ignore, indexed by symbol_name(+offset) or by absolute address. */
    std::vector<std::string> ignored_pcs;
};

/* Globals */
extern struct core_knobs_t core_knobs;
extern struct uncore_knobs_t uncore_knobs;
extern struct system_knobs_t system_knobs;

#endif /* __KNOBS_H__ */
