/* Definitions of scalar and distribution statistic types.
 *
 * Supports the following data types:
 * - int, unsigned int, double, float, int64_t, uint64_t,
 *   const char*.
 *
 * Author: Sam Xi
 */

#ifndef __STATISTIC_H__
#define __STATISTIC_H__

#include <string>

#include "boost_statistics.h"

#include "host.h"

// NOTE: Temporary.
const int PF_COUNT = 0x0001;
const int PF_PDF = 0x0002;
const int PF_CDF = 0x0004;
const int PF_ALL = (PF_COUNT | PF_PDF | PF_CDF);

namespace xiosim {
namespace stats {

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
        this->print = print;
        this->scale = scale;
        if (output_fmt)
            this->output_fmt = output_fmt;
        else
            this->output_fmt = "";
    }

    BaseStatistic(const BaseStatistic& stat)
        : name(stat.name)
        , desc(stat.desc)
        , output_fmt(stat.output_fmt)
        , print(stat.print)
        , scale(stat.scale) {}

    virtual ~BaseStatistic() {}

    std::string get_name() { return name; }
    std::string get_desc() { return desc; }
    std::string get_output_fmt() { return output_fmt; }
    bool is_printed() { return print; }
    bool is_scaled() { return scale; }
    void set_printed(bool print) { this->print = print; }
    void set_scaled(bool scale) { this->scale = scale; }
    void set_output_fmt(std::string output_fmt) { this->output_fmt = output_fmt; }

    /* Print the statistic to the file descriptor fd according to the
     * output_fmt string if @print is true. */
    virtual void print_value(FILE* fd) = 0;

    /* Scale the final statistic value by @weight if @scale is true. */
    virtual void scale_value(double weight) = 0;

    /* Accumulate the final value(s) of another stat into the current final value. */
    virtual void accum_stat(BaseStatistic* other_stat) = 0;

    /* Saves the current value as the final value. */
    virtual void save_value() = 0;

    /* Saves the difference of the current and initial value as the final value. */
    virtual void save_delta() = 0;

  protected:
    std::string name;        // Statistic name.
    std::string desc;        // Statistic description.
    std::string output_fmt;  // fprintf format string for value.
    bool print;              // If true, prints the value upon calling print_value().
    bool scale;              // If true, scales the value upon calling scale_value().
};

/* Base single-value statistic object interface. This is specialized for
 * arithmetic types and non-arithmetic types. */
template <typename V>
class StatisticCommon : public BaseStatistic {
  public:
    StatisticCommon(const char* name,
                    const char* desc,
                    const char* output_fmt = "",
                    bool print = true,
                    bool scale = true)
        : BaseStatistic(name, desc, output_fmt, print, scale) {}

    StatisticCommon(const StatisticCommon<V>& stat) : BaseStatistic(stat) {}

    /* File descriptors are used instead of fstream objects because all format
     * strings were originally written for fprintf, and it's not worth changing
     * all of them to C++ stream manipulation semantics.
     */
    virtual void print_value(FILE* fd) = 0;

    /* Accessors. */
    virtual V get_value() const = 0;
    virtual V get_init_val() const = 0;
    virtual V get_final_val() const = 0;

    /* Some statistic types cannot be scaled, accumulated, or delta-saved (e.g.
     * strings), and we need to specialize for those cases.
     */
    virtual void scale_value(double weight) = 0;
    virtual void accum_stat(BaseStatistic* other) = 0;
    virtual void save_value() = 0;
    virtual void save_delta() = 0;
};

/* Specialization for nonarithmetic types. Primary differences: they are not
 * initialized to an initial value and they cannot be scaled, accumulated, or
 * delta-saved.
 */
template <typename V, typename enable = void>
class Statistic : public StatisticCommon<V> {
  public:
    Statistic(const char* name,
              const char* desc,
              V val,
              const char* output_fmt = "%12s",
              bool print = true,
              bool scale = true)
        : StatisticCommon<V>(name, desc, output_fmt, print, scale)
        , value(val) {
        if (this->output_fmt.empty())
            set_output_format_default();
    }

    Statistic(const Statistic<V>& stat)
        : StatisticCommon<V>(stat)
        , value(stat.value) {
        if (this->output_fmt.empty())
            set_output_format_default();
    }

    virtual V get_value() const { return value; }
    virtual V get_init_val() const { return value; }
    virtual V get_final_val() const { return value; }

    virtual void print_value(FILE* fd) {
        if (!this->print)
            return;
        fprintf(fd, "%-28s", this->name.c_str());
        fprintf(fd, this->output_fmt.c_str(), value);
        if (!this->desc.empty())
            fprintf(fd, " # %s", this->desc.c_str());
        fprintf(fd, "\n");
    }

    /* Set the default output format if the provided output_fmt is empty or
     * NULL. */
    virtual void set_output_format_default() { this->output_fmt = "%12s"; }

    // For nonarithmetic types, scaling, accumulating, and saves don't apply.
    virtual void scale_value(double weight) {}
    virtual void accum_stat(BaseStatistic* other) {}
    virtual void save_value() {}
    virtual void save_delta() {}

  protected:
    const V value;     // Immutable copy of the value;
};

/* Specialization for arithmetic types. Distinguishing factors: arithmetic types
 * are initialized to an initial value, and they can be scaled.
 */
template <typename V>
class Statistic<
        V,
        typename boost::enable_if<boost::is_arithmetic<V>>::type> : public StatisticCommon<V> {

  public:
    Statistic(const char* name,
              const char* desc,
              V* val,
              V init_val,
              const char* output_fmt = "",
              bool print = true,
              bool scale = true)
        : StatisticCommon<V>(name, desc, output_fmt, print, scale)
        , value(val)
        , init_val(init_val)
        , final_val(init_val) {
        *(this->value) = init_val;
        if (this->output_fmt.empty())
            set_output_format_default();
    }

    Statistic(const Statistic<V>& stat)
        : StatisticCommon<V>(stat)
        , value(stat.value) {
        if (this->output_fmt.empty())
            set_output_format_default();
    }

    virtual V get_value() const { return *value; }
    virtual V get_init_val() const { return init_val; }
    virtual V get_final_val() const  { return final_val; }

    virtual void scale_value(double weight) { final_val = ((double)(final_val)) * weight; }

    virtual void accum_stat(BaseStatistic* other) {
        Statistic<V>* stat = static_cast<Statistic<V>*>(other);
        *value += stat->final_val;
    }

    /* Saves the current value as the final value. */
    void save_value() { final_val = *value; }

    virtual void save_delta() { final_val = *value - init_val; }

    virtual void print_value(FILE* fd) {
        if (!this->print)
            return;
        fprintf(fd, "%-28s", this->name.c_str());
        fprintf(fd, this->output_fmt.c_str(), *(this->value));
        fprintf(fd, " # %s\n", this->desc.c_str());
    }

    /* Different types of data have different output format defaults. SFINAE
     * is used heavily here to assign different format strings for different
     * template types. */

    template <typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, int>>::type* = 0) {
        this->output_fmt = "%12d";
    }

    template <typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, unsigned int>>::type* = 0) {
        this->output_fmt = "%12u";
    }

    template <typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, double>>::type* = 0) {
        this->output_fmt = "%12.4f";
    }

    template <typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, float>>::type* = 0) {
        this->output_fmt = "%12.4f";
    }

    /* signed qword. */
    template <typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, int64_t>>::type* = 0) {
        this->output_fmt = "%12" PRId64;
    }

    /* unsigned qword. */
    template <typename U = V>
    void set_output_format_default(
            typename boost::enable_if<boost::is_same<U, uint64_t>>::type* = 0) {
        this->output_fmt = "%12" PRIu64;
    }

  protected:
    V* value;     // Pointer to allocated memory storing the value.
    V init_val;   // Initial value to assign the statistic.
    V final_val;  // Final value of the statistic. Used to compare deltas.
};

/* Distribution statistic.
 * TODO: Add a copy constructor.
 * */
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
                 bool scale = true)
        : BaseStatistic(name, desc, output_fmt, print, scale) {
        this->init_val = init_val;
        this->stat_labels = stat_labels;
        this->array_sz = array_sz;
        this->bucket_sz = bucket_sz;
        this->overflows = 0;
        this->array = new unsigned int[array_sz];
        this->final_array = new unsigned int[array_sz];
        for (unsigned int i = 0; i < array_sz; i++) {
            this->array[i] = init_val;
            this->final_array[i] = init_val;
        }
    }

    ~Distribution() {
        delete[] array;
        delete[] final_array;
    }

    unsigned int get_overflows() { return overflows; }

    /* Add samples to the distribution with value @value. @value is an int that
     * is treated as an index into the distribution array. num_samples defaults
     * to 1 if unspecified. */
    void add_samples(unsigned int value, unsigned int num_samples = 1) {
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
                        features<tag::mean, tag::moment<2>, tag::min, tag::max, tag::count>> acc;
        for (unsigned int i = 0; i < array_sz; i++)
            acc(array[i]);

        // Despite the using declaration, there are some naming conflicts from
        // STL that require us to fully qualify some of these functions.
        stats->count = boost::accumulators::count(acc);
        stats->sum = boost::accumulators::sum(acc);
        stats->min = boost::accumulators::min(acc);
        stats->max = boost::accumulators::max(acc);
        stats->mean = boost::accumulators::mean(acc);
        stats->variance = boost::accumulators::moment<2>(acc);
        stats->stddev = sqrt(stats->variance);
    }

    // Scales each element in the distribution.
    virtual void scale_value(double weight) {
        for (unsigned int i = 0; i < array_sz; i++) {
            array[i] = array[i] * weight;
        }
    }

    virtual void accum_stat(BaseStatistic* other) {
        Distribution* dist = static_cast<Distribution*>(other);
        for (unsigned int i = 0; i < array_sz; i++)
            array[i] += dist->array[i];
    }

    virtual void save_value() {
        for (unsigned int i = 0; i < array_sz; i++)
            final_array[i] = array[i];
    }

    virtual void save_delta() {
        for (unsigned int i = 0; i < array_sz; i++)
            final_array[i] = array[i] - init_val;
    }

    void print_value(FILE* fd) {
        dist_stats_t stats;
        compute_dist_stats(&stats);

        fprintf(fd, "%-28s # %s\n", name.c_str(), desc.c_str());
        fprintf(fd, "%s.array_sz = %u\n", name.c_str(), array_sz);
        fprintf(fd, "%s.bucket_sz = %u\n", name.c_str(), bucket_sz);
        fprintf(fd, "%s.count = %u\n", name.c_str(), stats.count);
        fprintf(fd, "%s.total = %d\n", name.c_str(), stats.sum);
        fprintf(fd, "%s.imin = %d\n", name.c_str(), stats.count > 0 ? stats.min : -1);
        fprintf(fd, "%s.imax = %d\n", name.c_str(), stats.count > 0 ? stats.max : -1);
        fprintf(fd, "%s.average = %8.4f\n", name.c_str(), stats.mean);
        fprintf(fd, "%s.std_dev = %8.4f\n", name.c_str(), stats.stddev);
        fprintf(fd, "%s.overflows = %u\n", name.c_str(), overflows);
        fprintf(fd, "# pdf = prob. dist. fn. cdf = cumulative dist. fn.\n");
        fprintf(fd, "# %14s %10s %6s %6s \n", "index", "count", "pdf", "cdf");
        fprintf(fd, "%s.start_dist\n", name.c_str());

        bool has_custom_fmt = !output_fmt.empty();
        const char* index_fmt = "%16u ";
        const char* count_fmt = "%10u ";
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
            fprintf(fd, pdf_fmt, (double)array[i] / fmax(stats.sum, 1.0) * 100.0);
            fprintf(fd, cdf_fmt, cdf / fmax(stats.sum, 1.0) * 100.0);
            fprintf(fd, "\n");
        }
        fprintf(fd, "%s.end_dist\n", name.c_str());
    }

  private:
    unsigned int* array;        // Distribution array.
    unsigned int* final_array;  // Final distribution values.
    unsigned int array_sz;      // Array size.
    unsigned int overflows;     // Store values beyond the size of the array.
    unsigned int bucket_sz;     // Array bucket size.
    unsigned int init_val;      // Initial value for all elements in the distribution.
    // Labels for each element in the distribution. Corresponds to array index
    // for index.
    const char** stat_labels;
};

}  // namespace stats
}  // namespace xiosim

#endif
