/* Statistics database library.
 *
 * This library tracks scalar, distribution, and formula type statistics.
 *
 * Scalar types supported:
 *  - int, unsigned int, double, float, long long, unsigned long long, const
 *    char*
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

#include "statistic.h"
#include "expression.h"

namespace xiosim {
namespace stats {

/* Database of all statistics. Supports only the statistic types named in the
 * header comments.
 */
class StatsDatabase {
  public:
    StatsDatabase() { slice_weight = 1.0; }

    ~StatsDatabase() {
        for (auto& stat_kv : database)
            delete stat_kv.second;
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
        database[name] = stat;
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
        database[name] = stat;
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
        database[name] = stat;
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
        database[name] = stat;
        return stat;
    }

    Statistic<long long>* add_statistic(const char* name,
                                        const char* desc,
                                        long long* value,
                                        long long init_val,
                                        const char* output_fmt = "%12lld",
                                        bool print = true,
                                        bool scale = true) {
        Statistic<long long>* stat =
                new Statistic<long long>(name, desc, value, init_val, output_fmt, print, scale);
        database[name] = stat;
        return stat;
    }

    Statistic<unsigned long long>* add_statistic(const char* name,
                                                 const char* desc,
                                                 unsigned long long* value,
                                                 unsigned long long init_val,
                                                 const char* output_fmt = "%12lu",
                                                 bool print = true,
                                                 bool scale = true) {
        Statistic<unsigned long long>* stat = new Statistic<unsigned long long>(
                name, desc, value, init_val, output_fmt, print, scale);
        database[name] = stat;
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
        database[name] = stat;
        return stat;
    }

    Distribution* add_distribution(const char* name,
                                   const char* desc,
                                   unsigned init_val,
                                   unsigned int array_sz,
                                   unsigned int bucket_sz,
                                   const char** stat_labels = NULL,
                                   const char* output_fmt = "",
                                   bool print = true,
                                   bool scale = true) {
        Distribution* dist = new Distribution(
                name, desc, init_val, array_sz, bucket_sz, stat_labels, output_fmt, print, scale);
        database[name] = dist;
        return dist;
    }

    Formula* add_formula(const char* name,
                         const char* desc,
                         std::unique_ptr<Expression> expression,
                         const char* output_fmt = "%12.4f",
                         bool print = true,
                         bool scale = true) {
        Formula* formula = new Formula(name, desc, output_fmt, print, scale);
        *formula = expression;
        database[name] = formula;
        return formula;
    }

    /* Add an existing formula object.
     *
     * The new formula will be move constructed from the given formula.
     */
    Formula* add_formula(Formula& formula) {
        Formula* f = new Formula(std::move(formula));
        database[f->get_name()] = f;
        return f;
    }

    BaseStatistic* get_statistic(std::string stat_name) {
        if (database.find(stat_name) != database.end())
            return database[stat_name];
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
        if (database.find(name) != database.end())
            print_stat_object(database[name], fd);
    }

    // Print all statistics to the specified file descriptor.
    void print_all_stats(FILE* fd) {
        for (auto& stat_kv : database) {
            print_stat_object(stat_kv.second, fd);
        }
    }

    // Scale all stats by slice_weight.
    void scale_all_stats() {
        for (auto& stat_kv : database) {
            stat_kv.second->scale_value(slice_weight);
        }
    }

    // Save all statistics in the database.
    void save_stats_values() {
        for (auto& stat_kv : database) {
            stat_kv.second->save_value();
        }
    }

    void save_stats_deltas() {
        for (auto& stat_kv : database) {
            stat_kv.second->save_delta();
        }
    }

    // Accumulate all corresponding statistics from this database and another
    // database into this database. If a statistic in the other database is not
    // found in this database, it is skipped, because statistic objects only
    // store pointers to preallocated variables; they cannot create new
    // variables to track.
    void accum_all_stats(StatsDatabase* other) {
        for (auto& stat_kv : other->database) {
            BaseStatistic* other_stat = stat_kv.second;
            BaseStatistic* this_stat;
            if (database.find(stat_kv.first) != database.end()) {
                this_stat = database[stat_kv.first];
                this_stat->accum_stat(other_stat);
            }
        }
    }

    // Weight to be applied to all statistics in this database.
    double slice_weight;

  private:
    /* To act as a heterogeneous container for Statistic<T>, Distribution, and
     * Formula types, this map stores pointers to BaseStatistic objects. From
     * the StatsDatabase class, all that is required of each BaseStatistic
     * object is the print_value() and scale_value() functions, which are
     * declared in BaseStatistic.
     */
    std::map<std::string, BaseStatistic*> database;
};

}  // namespace stats
}  // namespace xiosim

#endif
