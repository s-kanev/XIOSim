/* Statistics database library.
 *
 * This library tracks scalar, distribution, and formula type statistics.
 *
 * Scalar types supported:
 *  - int, unsigned int, double, float, int64_t, uint64_t, const char*
 * Distributions only support integer data.
 * Formulas support any scalar type of statistic. Constant values are supported
 * as well (see details in expression.h).
 *
 * Author: Sam Xi.
 */

#ifndef __STAT_DATABASE_H__
#define __STAT_DATABASE_H__

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "statistic.h"
#include "expression_impl.h"

namespace xiosim {
namespace stats {

/* Database of all statistics. Supports only the statistic types named in the
 * header comments.
 */
class StatsDatabase {
  public:
    StatsDatabase() { slice_weight = 1.0; }

    ~StatsDatabase() {
        for (auto stat : stat_list)
            delete stat;
    }

    /* Statistic registration functions. Each function's signature is the same
     * as the corresponding constructor for the template type. Every add_*
     * function returns a pointer to the newly created statistic.
     *
     * This applies to add_distribution() and add_formula() as well.
     */

    Statistic<int>* add_statistic(const char* name,
                                  const char* desc,
                                  int* value,
                                  int init_val,
                                  const char* output_fmt = "%12d",
                                  bool print = true,
                                  bool scale = true) {
        Statistic<int>* stat =
                new Statistic<int>(name, desc, value, init_val, output_fmt, print, scale);
        stat_list.push_back(stat);
        stat_db[name] = stat_list.size() - 1;
        return stat;
    }

    Statistic<unsigned int>* add_statistic(const char* name,
                                           const char* desc,
                                           unsigned int* value,
                                           unsigned int init_val,
                                           const char* output_fmt = "%12u",
                                           bool print = true,
                                           bool scale = true) {
        Statistic<unsigned int>* stat =
                new Statistic<unsigned int>(name, desc, value, init_val, output_fmt, print, scale);
        stat_list.push_back(stat);
        stat_db[name] = stat_list.size() - 1;
        return stat;
    }

    Statistic<double>* add_statistic(const char* name,
                                     const char* desc,
                                     double* value,
                                     double init_val,
                                     const char* output_fmt = "%12.4f",
                                     bool print = true,
                                     bool scale = true) {
        Statistic<double>* stat =
                new Statistic<double>(name, desc, value, init_val, output_fmt, print, scale);
        stat_list.push_back(stat);
        stat_db[name] = stat_list.size() - 1;
        return stat;
    }

    Statistic<float>* add_statistic(const char* name,
                                    const char* desc,
                                    float* value,
                                    float init_val,
                                    const char* output_fmt = "%12.4f",
                                    bool print = true,
                                    bool scale = true) {
        Statistic<float>* stat =
                new Statistic<float>(name, desc, value, init_val, output_fmt, print, scale);
        stat_list.push_back(stat);
        stat_db[name] = stat_list.size() - 1;
        return stat;
    }

    Statistic<int64_t>* add_statistic(const char* name,
                                        const char* desc,
                                        int64_t* value,
                                        int64_t init_val,
                                        const char* output_fmt = "%12lld",
                                        bool print = true,
                                        bool scale = true) {
        Statistic<int64_t>* stat =
                new Statistic<int64_t>(name, desc, value, init_val, output_fmt, print, scale);
        stat_list.push_back(stat);
        stat_db[name] = stat_list.size() - 1;
        return stat;
    }

    Statistic<uint64_t>* add_statistic(const char* name,
                                                 const char* desc,
                                                 uint64_t* value,
                                                 uint64_t init_val,
                                                 const char* output_fmt = "%12lu",
                                                 bool print = true,
                                                 bool scale = true) {
        Statistic<uint64_t>* stat = new Statistic<uint64_t>(
                name, desc, value, init_val, output_fmt, print, scale);
        stat_list.push_back(stat);
        stat_db[name] = stat_list.size() - 1;
        return stat;
    }

    Statistic<const char*>* add_statistic(const char* name,
                                          const char* desc,
                                          const char* value,
                                          const char* output_fmt = "%12s",
                                          bool print = true,
                                          bool scale = false) {
        Statistic<const char*>* stat =
                new Statistic<const char*>(name, desc, value, output_fmt, print, scale);
        stat_list.push_back(stat);
        stat_db[name] = stat_list.size() - 1;
        return stat;
    }

    Distribution* add_distribution(const char* name,
                                   const char* desc,
                                   unsigned init_val,
                                   unsigned int array_sz,
                                   const char** stat_labels = NULL,
                                   const char* output_fmt = "",
                                   bool print = true,
                                   bool scale = true) {
        Distribution* dist = new Distribution(
                name, desc, init_val, array_sz, stat_labels, output_fmt, print, scale);
        stat_list.push_back(dist);
        stat_db[name] = stat_list.size() - 1;
        return dist;
    }

    SparseHistogram* add_sparse_histogram(const char* name,
                                          const char* desc,
                                          const char* label_fmt = "",
                                          const char* output_fmt = "",
                                          bool print = true,
                                          bool scale = true) {
        SparseHistogram* hist = new SparseHistogram(
                name, desc, label_fmt, output_fmt, print, scale);
        stat_list.push_back(hist);
        stat_db[name] = stat_list.size() - 1;
        return hist;
    }

    Formula* add_formula(const char* name,
                         const char* desc,
                         const Expression& expression,
                         const char* output_fmt = "%12.4f",
                         bool print = true,
                         bool scale = true) {
        Formula* formula = new Formula(name, desc, output_fmt, print, scale);
        *formula = expression;
        stat_list.push_back(formula);
        stat_db[name] = stat_list.size() - 1;
        return formula;
    }

    /* Add an existing formula object.
     */
    Formula* add_formula(const Formula& formula) {
        Formula* f = new Formula(formula);
        stat_list.push_back(f);
        stat_db[f->get_name()] = stat_list.size() - 1;
        return f;
    }

    BaseStatistic* get_statistic(std::string stat_name) {
        if (stat_db.find(stat_name) != stat_db.end())
            return stat_list[stat_db[stat_name]];
        return nullptr;
    }

    // Prints the contents of the statistic if the statistic's print property is
    // true.
    void inline print_stat_object(BaseStatistic* stat, FILE* fd) {
        if (stat->is_printed())
            stat->print_value(fd);
    }

    // Convenience method for printing by stat name.
    void print_stat_by_name(std::string name, FILE* fd) {
        BaseStatistic* stat = get_statistic(name);
        if (stat)
            print_stat_object(stat, fd);
    }

    // Print all statistics to the specified file descriptor.
    void print_all_stats(FILE* fd) {
        for (auto stat: stat_list) {
            print_stat_object(stat, fd);
        }
    }

    // Scale all stats by slice_weight.
    void scale_all_stats() {
        for (auto stat : stat_list) {
            stat->scale_value(slice_weight);
        }
    }

    // Save all statistics in the database.
    void save_stats_values() {
        for (auto stat : stat_list) {
            stat->save_value();
        }
    }

    void save_stats_deltas() {
        for (auto stat : stat_list) {
            stat->save_delta();
        }
    }

    // Accumulate all corresponding statistics from this database and another
    // database into this database. If a statistic in the other database is not
    // found in this database, it is skipped, because statistic objects only
    // store pointers to preallocated variables; they cannot create new
    // variables to track.
    void accum_all_stats(StatsDatabase* other) {
        for (auto other_stat : other->stat_list) {
            std::string other_string_name = other_stat->get_name();
            if (stat_db.find(other_string_name) != stat_db.end()) {
                BaseStatistic* this_stat = stat_list[stat_db[other_string_name]];
                this_stat->accum_stat(other_stat);
            }
        }
    }

    // Weight to be applied to all statistics in this database.
    double slice_weight;

  private:
    /* To act as a heterogeneous container for Statistic<T>, Distribution, and
     * Formula types, this vector stores pointers to BaseStatistic objects. From
     * the StatsDatabase class, all that is required of each BaseStatistic
     * object is the print_value() and scale_value() functions, which are
     * declared in BaseStatistic.
     *
     * We use a vector so that we can preserve the ordering of registration
     * calls when printing the entire database.
     */
    std::vector<BaseStatistic*> stat_list;

    /* Maps a statistic name to the position in the database vector for fast
     * access. */
    std::map<std::string, size_t> stat_db;

};

}  // namespace stats
}  // namespace xiosim

#endif
