/* Statistics database library. Supports tracking of the following data types:
 *  - int, unsigned int, double, float, long long, unsigned long long, const
 *    char*.
 * This also supports tracking of distribution statistics and formulas.
 *
 * Author: Sam Xi.
 */

#ifndef __STAT_DATABASE_H__
#define __STAT_DATABASE_H__

#include <cmath>
#include <cstdio>
#include <string>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/utility/enable_if.hpp>

/* Collection of distribution related statistics. */
struct dist_stats_t {
  unsigned int count;
  unsigned int sum;
  double mean;
  unsigned int min;
  unsigned int max;
  double stddev;
  double variance;
};

/* Base abstract statistic class. It stores only metadata about the statistic,
 * but not the value. */
class BaseStatistic {
  public:
    BaseStatistic(const char* name,
                  const char* desc,
                  const char* output_fmt = "",
                  bool print = true,
                  bool scale = true) {
      this->name = name;
      this->desc = desc;
      this->output_fmt = output_fmt;
      this->print = print;
      this->scale = scale;
    }

    virtual ~BaseStatistic() {}

    std::string get_name() { return name; }
    std::string get_desc() { return desc; }
    std::string get_output_fmt() { return output_fmt; }
    bool is_printed() { return print; }
    bool is_scaled() { return scale; }

    /* Print the statistic to the file descriptor fd according to the output_fmt
    * string if @print is true. */
    virtual void print_value(FILE* fd) = 0;

    /* Scale the statistic value by @weight if @scale is true. */
    virtual void scale_value(double weight) = 0;

  protected:
    std::string name;  // Statistic name.
    std::string desc;  // Statistic description.
    std::string output_fmt;  // fprintf format string for value.
    bool print;  // If true, prints the value upon calling print_value().
    bool scale;  // If true, scales the value upon calling scale_value().
};

/* Base single-value statistic class. These are specialized via SFINAE for
 * arithmetic types and non-arithmetic types. */
template<typename V>
class StatisticCommon : public BaseStatistic {
  public:
    StatisticCommon(const char* name,
                    const char* desc,
                    V* value,
                    const char* output_fmt = "",
                    bool print = true,
                    bool scale = true) :
        BaseStatistic(name, desc, output_fmt, print, scale) {
      this->value = value;
      this->output_fmt = output_fmt;
    }

    /* Returns the value stored at the stored address. */
    V get_value() { return *value; }
    /* Returns the initial value. */
    V get_init_val() { return init_val; }

    /* File descriptors are used instead of fstream objects because all format
     * strings were originally written for fprintf, and it's not worth changing
     * all of them to C++ stream manipulation semantics.
     */
    void print_value(FILE *fd) {
      if (!this->print)
        return;
      fprintf(fd, "%-28s", this->name.c_str());
      fprintf(fd, this->output_fmt.c_str(), *(this->value));
      fprintf(fd, " # %s\n", this->desc.c_str());
    }

    // Some statistic types cannot be scaled (e.g. strings).
    virtual void scale_value(double weight) = 0;

  protected:
    V* value;  // Pointer to allocated memory storing the value.
    V init_val;  // Initial value to assign the statistic.
};


/* Specialization for nonarithmetic types. Primary differences: they are not
 * initialized to an initial value and they cannot be scaled.
 */
template<typename V, typename enable = void>
class Statistic : public StatisticCommon<V> {
  public:
    Statistic(const char* name,
              const char* desc,
              V* value,
              const char* output_fmt = "%12s",
              bool print = true,
              bool scale = true) :
      StatisticCommon<V>(
          name, desc, value, output_fmt, print, scale) {
    }

    void scale_value(double weight) {}
};

/* Specialization for arithmetic types. Distinguishing factors: arithmetic types
 * are initialized to an initial value, and they can be scaled.
 */
template<typename V>
class Statistic<V, typename boost::enable_if<boost::is_arithmetic<V> >::type> :
    public StatisticCommon<V> {

  public:
    Statistic(const char* name,
              const char* desc,
              V* value,
              V init_val,
              const char* output_fmt = "",
              bool print = true,
              bool scale = true) : StatisticCommon<V>(
            name, desc, value, output_fmt, print, scale) {
      this->init_val = init_val;
      *(this->value) = init_val;
      if (this->output_fmt.empty())
        set_output_format_default();
    }

    void scale_value(double weight) {
      *(this->value) *= (V) weight;
    }

    /* Different types of data have different output format defaults. SFINAE
     * is used heavily here to assign different format strings for different
     * template types. */

    template<typename U = V>
    void set_output_format_default(
        typename boost::enable_if<boost::is_same<U, int> >::type* = 0) {
      this->output_fmt = "%12d";
    }

    template<typename U = V>
    void set_output_format_default(typename boost::enable_if<
            boost::is_same<U, unsigned int> >::type* = 0) {
      this->output_fmt = "%12u";
    }

    template<typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, double> >::type* = 0) {
      this->output_fmt = "%12.4f";
    }

    template<typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, float> >::type* = 0) {
      this->output_fmt = "%12.4f";
    }

    /* signed qword. */
    template<typename U = V>
    void set_output_format_default(typename boost::enable_if<
            boost::is_same<U, long long> >::type* = 0) {
      this->output_fmt = "%12lld";
    }

    /* unsigned qword. */
    template<typename U = V>
    void set_output_format_default(typename boost::enable_if<
            boost::is_same<U, unsigned long long> >::type* = 0) {
      this->output_fmt = "%12lu";
    }
};

/* Distribution statistic. */
class Distribution : public BaseStatistic {
  public:
    Distribution(const char* name,
                 const char* desc,
                 unsigned int init_val,
                 unsigned int array_sz,
                 unsigned int bucket_sz,
                 const char** stat_labels = NULL,
                 const char* output_fmt = "",
                 bool print = true,
                 bool scale = true) :
          BaseStatistic(name, desc, output_fmt, print, scale) {
      this->init_val = init_val;
      this->stat_labels = stat_labels;
      this->array_sz = array_sz;
      this->bucket_sz = bucket_sz;
      this->overflows = 0;
      this->array = new unsigned int[array_sz];
      for (unsigned int i = 0; i < array_sz; i++)
        this->array[i] = init_val;
    }

    ~Distribution() {
      delete[] array;
    }

    unsigned int get_overflows() { return overflows; }

    /* Add samples to the distribution with value @value. @value is an int that
     * is treated as an index into the distribution array. num_samples defaults
     * to 1 if unspecified. */
    void add_samples(unsigned int value, unsigned int num_samples=1) {
      unsigned int arr_idx = value / bucket_sz;
      if (arr_idx < array_sz)
        array[arr_idx] += num_samples;
      else
        overflows += num_samples;
    }

    // Computes various distribution related statistics.
    void compute_dist_stats(dist_stats_t* stats) {
      using namespace boost::accumulators;
      accumulator_set<unsigned int,
                      features<tag::mean, tag::moment<2>,
                               tag::min, tag::max, tag::count> > acc;
      for (unsigned int i = 0; i < array_sz; i++)
        acc(array[i]);
      stats->count = count(acc);
      stats->sum = sum(acc);
      stats->min = min(acc);
      stats->max = max(acc);
      stats->mean = mean(acc);
      stats->variance = moment<2>(acc);
      stats->stddev = sqrt(stats->variance);
    }

    // Scales each element in the distribution.
    void scale_value(double weight) {
      for (unsigned int i = 0; i < array_sz; i++) {
        array[i] = array[i] * weight;
      }
    }

    void print_value(FILE* fd) {
      dist_stats_t stats;
      compute_dist_stats(&stats);

      fprintf(fd, "\n");
      fprintf(fd, "%-28s # %s\n", name.c_str(), desc.c_str());
      fprintf(fd, "%s.array_sz = %u\n", name.c_str(), array_sz);
      fprintf(fd, "%s.bucket_sz = %u\n", name.c_str(), bucket_sz);
      fprintf(fd, "%s.count = %u\n", name.c_str(), stats.count);
      fprintf(fd, "%s.total = %d\n", name.c_str(), stats.sum);
      fprintf(fd, "%s.imin = %d\n", name.c_str(),
              stats.count > 0 ? stats.min : -1);
      fprintf(fd, "%s.imax = %d\n", name.c_str(),
              stats.count > 0 ? stats.max : -1);
      fprintf(fd, "%s.average = %8.4f\n", name.c_str(), stats.mean);
      fprintf(fd, "%s.std_dev = %8.4f\n", name.c_str(), stats.stddev);
      fprintf(fd, "%s.overflows = %u\n", name.c_str(), overflows);
      fprintf(fd, "# pdf = prob. dist. fn. cdf = cumulative dist. fn.\n");
      fprintf(fd, "# %14s %10s %6s %6s \n", "index", "count", "pdf", "cdf");
      fprintf(fd, "%s.start_dist\n", name.c_str());

      bool has_custom_fmt = !output_fmt.empty();
      const char* index_fmt = has_custom_fmt ? output_fmt.c_str() : "%16u ";
      const char* count_fmt = has_custom_fmt ? output_fmt.c_str() : "%10u ";
      const char* pdf_fmt = has_custom_fmt ? output_fmt.c_str() : "%6.2f ";
      const char* cdf_fmt = has_custom_fmt ? output_fmt.c_str() : "%6.2f ";
      double cdf = 0.0;
      for (unsigned int i = 0; i < array_sz; i++) {
        cdf += array[i];
        if (stat_labels)  // Has custom labels.
          fprintf(fd, "%-16s ", stat_labels[i]);
        else
          fprintf(fd, index_fmt, i * bucket_sz);
        fprintf(fd, count_fmt, array[i]);
        fprintf(fd, pdf_fmt,
                (double)array[i] / fmax(stats.sum, 1.0) * 100.0);
        fprintf(fd, cdf_fmt, cdf/fmax(stats.sum, 1.0) * 100.0);
        fprintf(fd, "\n");
      }
      fprintf(fd, "%s.end_dist\n", name.c_str());
    }

  private:
    unsigned int *array;  // Distribution array.
    unsigned int array_sz;  // Array size.
    unsigned int overflows;  // Store values beyond the size of the array.
    unsigned int bucket_sz;  // Array bucket size.
    unsigned int init_val;  // Initial value for all elements in the distribution.
    // Labels for each element in the distribution. Corresponds to array index
    // for index.
    const char **stat_labels;
};

// TODO: Hook up an expression evaluator.
class Formula : public BaseStatistic {
  public:
    Formula(const char* name,
            const char* desc,
            const char* formula,
            const char* output_fmt = "",
            bool print = true,
            bool scale = true) :
        BaseStatistic(name, desc, output_fmt, print, scale) {
      this->formula = formula;
    }

    void evaluate(double *value, bool* error) {
      // TODO: Implement...
    }

    void print_value(FILE* fd) {
      bool error = false;
      double value = 0.0;
      evaluate(&value, &error);
      if (error)
        fprintf(fd, "An error occurred parsing the formula %s", name.c_str());
      else
        fprintf(fd, output_fmt.c_str(), value);
      fprintf(fd, " # %s\n", desc.c_str());
    }

    // Formula statistics are not scaled.
    void scale_value(double weight) {
      return;
    }

  private:
    std::string formula;
};

/* Database of all statistics. Supports only the statistic types named in the
 * header comments.
 */
class StatsDatabase {
  public:
    StatsDatabase() {
      slice_weight = 1.0;
    }

    ~StatsDatabase() {
      for (auto it = database.begin(); it != database.end(); ++it)
        delete it->second;
    }

    /* Statistic registration functions. Each function's signature is the same
     * as the corresponding constructor for the template type. Every add_*
     * function returns a pointer to the newly created statistic.
     *
     * This applies to add_distribution() and add_formula() as well.
     */

   Statistic<int>* add_statistic(
        const char* name, const char* desc, int* value, int init_val,
        const char* output_fmt = "%12d", bool print = true, bool scale = true) {
      Statistic<int>* stat = new Statistic<int>(
          name, desc, value, init_val, output_fmt, print, scale);
      database[name] = stat;
      return stat;
    }

    Statistic<unsigned int>* add_statistic(
        const char* name, const char* desc, unsigned int* value,
        unsigned int init_val, const char* output_fmt = "%12u",
        bool print = true, bool scale = true) {
      Statistic<unsigned int>* stat = new Statistic<unsigned int>(
          name, desc, value, init_val, output_fmt, print, scale);
      database[name] = stat;
      return stat;
    }

    Statistic<double>* add_statistic(
        const char* name, const char* desc, double* value, double init_val,
        const char* output_fmt = "%12.4f", bool print = true,
        bool scale = true) {
      Statistic<double>* stat = new Statistic<double>(
          name, desc, value, init_val, output_fmt, print, scale);
      database[name] = stat;
      return stat;
    }

    Statistic<float>* add_statistic(
        const char* name, const char* desc, float* value, float init_val,
        const char* output_fmt = "%12.4f", bool print = true,
        bool scale = true) {
      Statistic<float>* stat = new Statistic<float>(
          name, desc, value, init_val, output_fmt, print, scale);
      database[name] = stat;
      return stat;
    }

    Statistic<long long>* add_statistic(
        const char* name, const char* desc, long long* value,
        long long init_val, const char* output_fmt = "%12lld",
        bool print = true, bool scale = true) {
      Statistic<long long>* stat = new Statistic<long long>(
          name, desc, value, init_val, output_fmt, print, scale);
      database[name] = stat;
      return stat;
    }

    Statistic<unsigned long long>* add_statistic(
        const char* name, const char* desc, unsigned long long* value,
        unsigned long long init_val, const char* output_fmt = "%12lu",
        bool print = true, bool scale = true) {
      Statistic<unsigned long long>* stat = new Statistic<unsigned long long>(
          name, desc, value, init_val, output_fmt, print, scale);
      database[name] = stat;
      return stat;
    }

    Statistic<const char*>* add_statistic(
        const char* name, const char* desc, const char** value,
        const char* output_fmt = "%12s", bool print = true, bool scale = true) {
      Statistic<const char*>* stat = new Statistic<const char*>(
          name, desc, value, output_fmt, print, scale);
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
          name, desc, init_val, array_sz, bucket_sz,
          stat_labels, output_fmt, print, scale);
      database[name] = dist;
      return dist;
    }

    Formula* add_formula(const char* name,
                         const char* desc,
                         const char* formula_expr,
                         const char* output_fmt = "",
                         bool print = true,
                         bool scale = true) {
      Formula* formula = new Formula(
          name, desc, formula_expr, output_fmt, print, scale);
      database[name] = formula;
      return formula;
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
      for (auto it = database.begin(); it != database.end(); ++it) {
        print_stat_object(it->second, fd);
      }
    }

    // Scale all stats by slice_weight.
    void scale_all_stats() {
      for (auto it = database.begin(); it != database.end(); ++it) {
        it->second->scale_value(slice_weight);
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

#endif
