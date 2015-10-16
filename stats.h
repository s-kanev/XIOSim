/* A temporary interface to the new statistics library.
 *
 * This preserves the original SimplerScalar API as much as possible to
 * simplify the migration. Formulas have to be specified differently, however,
 * since the new expression evaluator does not parse strings to build the
 * expression tree.
 *
 * Eventually, this should go away entirely.
 *
 * NOTE: stat_reg_counter -> stat_reg_sqword and stat_reg_addr -> stat_reg_uint
 * under machine.h, which is being phased out, so this needs to be updated
 * after the oracle purge is merged in.
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
#define stat_reg_core_counter stat_reg_core_sqword

typedef xiosim::stats::BaseStatistic BaseStatistic;
typedef xiosim::stats::Distribution Distribution;
typedef xiosim::stats::StatsDatabase StatsDatabase;
typedef xiosim::stats::Formula Formula;

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

xiosim::stats::Statistic<unsigned int>& stat_reg_uint(StatsDatabase* sdb,
                                                      int print_me,
                                                      const char* name,
                                                      const char* desc,
                                                      unsigned int* var,
                                                      unsigned int init_val,
                                                      int scale_me,
                                                      const char* format);

xiosim::stats::Statistic<qword_t>& stat_reg_qword(StatsDatabase* sdb,
                                                  int print_me,
                                                  const char* name,
                                                  const char* desc,
                                                  qword_t* var,
                                                  qword_t init_val,
                                                  int scale_me,
                                                  const char* format);

xiosim::stats::Statistic<qword_t>& stat_reg_core_qword(StatsDatabase* sdb,
                                                       int print_me,
                                                       int core_id,
                                                       const char* name,
                                                       const char* desc,
                                                       qword_t* var,
                                                       qword_t init_val,
                                                       int scale_me,
                                                       const char* format);

xiosim::stats::Statistic<sqword_t>& stat_reg_sqword(StatsDatabase* sdb,
                                                    int print_me,
                                                    const char* name,
                                                    const char* desc,
                                                    sqword_t* var,
                                                    sqword_t init_val,
                                                    int scale_me,
                                                    const char* format);

xiosim::stats::Statistic<sqword_t>& stat_reg_core_sqword(StatsDatabase* sdb,
                                                         int print_me,
                                                         int core_id,
                                                         const char* name,
                                                         const char* desc,
                                                         sqword_t* var,
                                                         sqword_t init_val,
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

Distribution* stat_reg_sdist(StatsDatabase* sdb,
                             const char* name,
                             const char* desc,
                             unsigned int init_val,
                             int pf,
                             const char* format,
                             int scale_me,
                             void* print_fn);

/* Add nsamples to array or sparse array distribution stat.
 *
 * dword_t = md_addr_t, but we haven't included machine.h at this point yet, so
 * we don't get that typedef (and it's only going to disappear anyways).
 *
 * Never used in the simulator.
 */
void stat_add_samples(BaseStatistic* stat, dword_t index, int nsamples);

/* Add a single sample to array or sparse array distribution STAT */
void stat_add_sample(BaseStatistic* stat, dword_t index);

/* Legacy.*/
Formula* stat_reg_formula(StatsDatabase* sdb,
                          int print_me,
                          const char* name,
                          const char* desc,
                          const char* formula,
                          const char* format);

/* Can we just use either ExpressionWrapper or Formula, but not both of them? */
Formula* stat_reg_formula(StatsDatabase* sdb,
                          int print_me,
                          const char* name,
                          const char* desc,
                          xiosim::stats::ExpressionWrapper expression,
                          const char* format);

Formula* stat_reg_core_formula(StatsDatabase* sdb,
                               int print_me,
                               int core_id,
                               const char* name,
                               const char* desc,
                               xiosim::stats::ExpressionWrapper expression,
                               const char* format);

Formula* stat_reg_formula(StatsDatabase* sdb, Formula& formula);

xiosim::stats::Statistic<const char*>& stat_reg_string(StatsDatabase* sdb,
                                                       const char* name,
                                                       const char* desc,
                                                       const char* var,
                                                       const char* format);

xiosim::stats::Statistic<const char*>& stat_reg_note(StatsDatabase* sdb, const char* note);

StatsDatabase* stat_new();

void stat_print_stats(StatsDatabase* sdb, FILE* fd);

void stat_print_stat(BaseStatistic* stat, FILE* fd);

template <typename V>
xiosim::stats::Statistic<V>* stat_find_stat(StatsDatabase* sdb,
                                            const char* stat_name) {
    using namespace xiosim::stats;
    BaseStatistic* base_stat = sdb->get_statistic(stat_name);
    Statistic<V>* stat = static_cast<Statistic<V>*>(base_stat);
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
    return stat;
}

void stat_find_dist(StatsDatabase* sdb, const char* stat_name, Distribution* dist);

void stat_find_core_dist(StatsDatabase* sdb,
                         int core_id,
                         const char* stat_name,
                         Distribution* dist);

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
