/* A temporary interface to the new statistics library.
 *
 * This preserves the original SimplerScalar API as much as possible to
 * simplify the migration. Formulas have to be specified differently, however,
 * since the new expression evaluator does not parse strings to build the
 * expression tree.
 *
 * Eventually, this should go away entirely.
 *
 * Author: Sam Xi
 */

#ifndef _STATS_REPL_H_
#define _STATS_REPL_H_

#include "host.h"
#include "statistic.h"
#include "expression.h"
#include "stat_database.h"

// Mimicking #define in machine.c.
#define stat_reg_counter stat_reg_sqword
#define stat_reg_core_counter stat_reg_core_sqword
#define stat_reg_cache_counter stat_reg_comp_sqword
#define stat_reg_cache_formula stat_reg_comp_formula
#define stat_reg_pred_counter stat_reg_comp_sqword
#define stat_reg_pred_formula stat_reg_comp_formula
#define stat_reg_pred_int stat_reg_comp_int
#define stat_reg_pred_string stat_reg_comp_string
#define stat_reg_comp_counter stat_reg_comp_sqword

using xiosim::stats::BaseStatistic;
using xiosim::stats::Distribution;
using xiosim::stats::StatsDatabase;
using xiosim::stats::Formula;
using xiosim::stats::ExpressionWrapper;

/* Returns the full stat name for a core component.
 *
 * Args:
 *    core_id: id of the core. If -1, then the core number part is omitted from the name.
 *    comp_name: name of the component.
 *    stat_name: name of the statistic
 *    full_name: preallocated buffer that is large enough to hold the full stat name.
 *    bool is_llc: true if the statistic is for the LLC, false otherwise.
 *        Defaults to false.
 *
 * Returns:
 *    Fully qualified stat name.
 */
void create_comp_stat_name(int core_id,
                           const char* comp_name,
                           const char* stat_name,
                           char* full_name,
                           bool is_llc=false);

xiosim::stats::Statistic<int>& stat_reg_int(StatsDatabase* sdb,
                                            int print_me,
                                            const char* name,
                                            const char* desc,
                                            int* var,
                                            int init_val,
                                            int scale_me,
                                            const char* format);

xiosim::stats::Statistic<int>& stat_reg_core_int(StatsDatabase* sdb,
                                                 int print_me,
                                                 int core_id,
                                                 const char* name,
                                                 const char* desc,
                                                 int* var,
                                                 int init_val,
                                                 int scale_me,
                                                 const char* format);

xiosim::stats::Statistic<int>& stat_reg_comp_int(StatsDatabase* sdb,
                                                 int print_me,
                                                 int core_id,
                                                 const char* comp_name,
                                                 const char* stat_name,
                                                 const char* desc,
                                                 int* var,
                                                 int init_val,
                                                 int scale_me,
                                                 const char* format);

xiosim::stats::Statistic<unsigned int>& stat_reg_uint(StatsDatabase* sdb,
                                                      int print_me,
                                                      const char* name,
                                                      const char* desc,
                                                      unsigned int* var,
                                                      unsigned int init_val,
                                                      int scale_me,
                                                      const char* format);

xiosim::stats::Statistic<uint64_t>& stat_reg_qword(StatsDatabase* sdb,
                                                  int print_me,
                                                  const char* name,
                                                  const char* desc,
                                                  uint64_t* var,
                                                  uint64_t init_val,
                                                  int scale_me,
                                                  const char* format);

xiosim::stats::Statistic<uint64_t>& stat_reg_core_qword(StatsDatabase* sdb,
                                                       int print_me,
                                                       int core_id,
                                                       const char* name,
                                                       const char* desc,
                                                       uint64_t* var,
                                                       uint64_t init_val,
                                                       int scale_me,
                                                       const char* format);

xiosim::stats::Statistic<int64_t>& stat_reg_sqword(StatsDatabase* sdb,
                                                    int print_me,
                                                    const char* name,
                                                    const char* desc,
                                                    int64_t* var,
                                                    int64_t init_val,
                                                    int scale_me,
                                                    const char* format);

xiosim::stats::Statistic<int64_t>& stat_reg_core_sqword(StatsDatabase* sdb,
                                                         int print_me,
                                                         int core_id,
                                                         const char* name,
                                                         const char* desc,
                                                         int64_t* var,
                                                         int64_t init_val,
                                                         int scale_me,
                                                         const char* format);

xiosim::stats::Statistic<int64_t>& stat_reg_comp_sqword(StatsDatabase* sdb,
                                                         int print_me,
                                                         int core_id,
                                                         const char* comp_name,
                                                         const char* stat_name,
                                                         const char* desc,
                                                         int64_t* var,
                                                         int64_t init_val,
                                                         int scale_me,
                                                         const char* format,
                                                         bool is_llc = false);

xiosim::stats::Statistic<int64_t>& stat_reg_pred_sqword(StatsDatabase* sdb,
                                                         int print_me,
                                                         int core_id,
                                                         const char* pred_name,
                                                         const char* stat_name,
                                                         const char* desc,
                                                         int64_t* var,
                                                         int64_t init_val,
                                                         int scale_me,
                                                         const char* format);

xiosim::stats::Statistic<float>& stat_reg_float(StatsDatabase* sdb,
                                                int print_me,
                                                const char* name,
                                                const char* desc,
                                                float* var,
                                                float init_val,
                                                int scale_me,
                                                const char* format);

xiosim::stats::Statistic<double>& stat_reg_double(StatsDatabase* sdb,
                                                  int print_me,
                                                  const char* name,
                                                  const char* desc,
                                                  double* var,
                                                  double init_val,
                                                  int scale_me,
                                                  const char* format);

xiosim::stats::Statistic<double>& stat_reg_comp_double(StatsDatabase* sdb,
                                                       int print_me,
                                                       int core_id,
                                                       const char* comp_name,
                                                       const char* stat_name,
                                                       const char* desc,
                                                       double* var,
                                                       double init_val,
                                                       int scale_me,
                                                       const char* format);

// Nothing actually uses the print_fn_t option, so we'll just leave it as a
// void* for now.
Distribution* stat_reg_dist(StatsDatabase* sdb,
                            const char* name,
                            const char* desc,
                            unsigned int init_val,
                            unsigned int arr_sz,
                            unsigned int bucket_sz,
                            int pf, /* print format, use PF_* defs */
                            const char* format,
                            const char** imap,
                            int scale_me,
                            void* print_fn);

Distribution* stat_reg_core_dist(StatsDatabase* sdb,
                                 int core_id,
                                 const char* name,
                                 const char* desc,
                                 unsigned int init_val,
                                 unsigned int arr_sz,
                                 unsigned int bucket_sz,
                                 int pf, /* print format, use PF_* defs */
                                 const char* format,
                                 const char** imap,
                                 int scale_me,
                                 void* print_fn);

Distribution* stat_reg_sdist(StatsDatabase* sdb,
                             const char* name,
                             const char* desc,
                             unsigned int init_val,
                             int pf,
                             const char* format,
                             int scale_me,
                             void* print_fn);

/* Registers some named queue's occupancy statistics.
 *
 * The name of the stats will be <queue_name>_occupancy, <queue_name>_empty,
 * and  <queue_name>_full, for occupancy, cycles_empty, and cycles_full,
 * respectively. In addition, each stat will register an associated Formula
 * that divides the stat by the total simulated cycles.
 */
void reg_core_queue_occupancy_stats(xiosim::stats::StatsDatabase* sdb,
                                    int core_id,
                                    std::string queue_name,
                                    counter_t* occupancy,
                                    counter_t* cycles_empty,
                                    counter_t* cycles_full);

/* Add a single sample to array or sparse array distribution STAT */
void stat_add_sample(BaseStatistic* stat, unsigned int index);

Formula* stat_reg_formula(StatsDatabase* sdb,
                          int print_me,
                          const char* name,
                          const char* desc,
                          ExpressionWrapper expression,
                          const char* format);

Formula* stat_reg_core_formula(StatsDatabase* sdb,
                               int print_me,
                               int core_id,
                               const char* name,
                               const char* desc,
                               ExpressionWrapper expression,
                               const char* format);

Formula* stat_reg_comp_formula(StatsDatabase* sdb,
                               int print_me,
                               int core_id,
                               const char* comp_name,
                               const char* stat_name,
                               const char* desc,
                               ExpressionWrapper expression,
                               const char* format,
                               bool is_llc = false);

Formula* stat_reg_formula(StatsDatabase* sdb, Formula& formula);

xiosim::stats::Statistic<const char*>& stat_reg_string(StatsDatabase* sdb,
                                                       const char* name,
                                                       const char* desc,
                                                       const char* var,
                                                       const char* format);

xiosim::stats::Statistic<const char*>& stat_reg_comp_string(StatsDatabase* sdb,
                                                            int core_id,
                                                            const char* comp_name,
                                                            const char* stat_name,
                                                            const char* desc,
                                                            const char* var,
                                                            const char* format);

xiosim::stats::Statistic<const char*>& stat_reg_note(StatsDatabase* sdb, const char* note);

StatsDatabase* stat_new();

void stat_print_stats(StatsDatabase* sdb, FILE* fd);

void stat_print_stat(BaseStatistic* stat, FILE* fd);

// TODO: Make this return a reference instead of a pointer, so we can avoid
// using dereferences in the reg_stats code.
template <typename V>
xiosim::stats::Statistic<V>* stat_find_stat(StatsDatabase* sdb,
                                            const char* stat_name) {
    using namespace xiosim::stats;
    BaseStatistic* base_stat = sdb->get_statistic(stat_name);
    Statistic<V>* stat = static_cast<Statistic<V>*>(base_stat);
    assert(stat && "The statistic could not be found!");
    return stat;
}

template <typename V>
xiosim::stats::Statistic<V>* stat_find_core_stat(StatsDatabase* sdb,
                                                 int core_id,
                                                 const char* stat_name) {
    using namespace xiosim::stats;
    const size_t STAT_NAME_LEN = 64;
    char full_stat_name[STAT_NAME_LEN];
    snprintf(full_stat_name, STAT_NAME_LEN, "c%d.%s", core_id, stat_name);
    Statistic<V>* stat = stat_find_stat<V>(sdb, full_stat_name);
    assert(stat && "The statistic could not be found!");
    return stat;
}

Distribution* stat_find_dist(StatsDatabase* sdb, const char* stat_name);

Distribution* stat_find_core_dist(StatsDatabase* sdb, int core_id, const char* stat_name);

Formula* stat_find_core_formula(StatsDatabase* sdb, int core_id, const char* stat_name);

template <typename V>
void stat_save_stat(xiosim::stats::Statistic<V>* stat) {
    stat->save_value();
}

void stat_save_stats(StatsDatabase* sdb);

void stat_save_stats_delta(StatsDatabase* sdb);

template <typename V>
void stat_save_stat_delta(xiosim::stats::Statistic<V>* stat);

void stat_scale_stats(StatsDatabase* sdb);

void stat_scale_stat(BaseStatistic* stat, double weight);

template <typename V>
void stat_accum_stat(xiosim::stats::Statistic<V>* dest_stat,
                     xiosim::stats::Statistic<V>* src_stat) {
    dest_stat->accum_stat(src_stat);
}

#endif
