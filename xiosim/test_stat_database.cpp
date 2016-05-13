/* Unit tests for the stats database replacement.
 *
 * Author: Sam Xi
 */

#include "catch.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "stat_database.h"
#include "stats.h"

using namespace xiosim::stats;
const std::string XIOSIM_PACKAGE_PATH = "xiosim/";

/* Opens a new randomly named temporary file for testing printfs.
 *
 * The name of the new randomly generated file is placed into temp_file_name.
 */
FILE* open_temp_file(char* temp_file_name) {
    char buf[21] = "/tmp/tmp_stat_XXXXXX";  // We know exactly how long the name will be.
    int fd = mkstemp(buf);
    REQUIRE(fd != -1);
    FILE* stream = fdopen(fd, "w+");
    REQUIRE(stream != NULL);
    strncpy(temp_file_name, buf, 21);  // Copy back into local buffer.
    return stream;
}

/* Closes the temp file descriptor and deletes the file. */
void cleanup_temp_file(FILE* temp_file, char* temp_file_name) {
    fclose(temp_file);
    std::remove(temp_file_name);
}

bool check_printfs(FILE* test_file, const std::string golden_file_name) {
    // Rewind the test file stream to the beginning so we can start reading.
    rewind(test_file);

    FILE* golden_file = fopen(golden_file_name.c_str(), "r");
    REQUIRE(golden_file != NULL);

    const int MAX_LINE_LENGTH = 200;
    char* golden_buf = new char[MAX_LINE_LENGTH];
    char* test_buf = new char[MAX_LINE_LENGTH];
    size_t golden_len = MAX_LINE_LENGTH;
    size_t test_len = MAX_LINE_LENGTH;
    ssize_t golden_read = 0;
    ssize_t test_read = 0;

    int line_num = 0;

    // Read first line. We have to make sure that both streams read the same
    // number of lines, or the EOF check at the end won't work.
    golden_read = getline(&golden_buf, &golden_len, golden_file);
    test_read = getline(&test_buf, &test_len, test_file);
    while (golden_read != -1 && test_read != -1) {
        bool lines_equal = (strncmp(golden_buf, test_buf, golden_len) == 0);
        if (!lines_equal) {
            // Print lines to the console for debugging BEFORE the REQUIRE macro.
            fprintf(stderr, "DIFFERENCE ON LINE %d.\n", line_num);
            fprintf(stderr, "Test output    : %s\n", test_buf);
            fprintf(stderr, "Correct output : %s\n", golden_buf);
            REQUIRE(lines_equal);
        }
        line_num++;
        golden_read = getline(&golden_buf, &golden_len, golden_file);
        test_read = getline(&test_buf, &test_len, test_file);
    }

    // If the test really passed, then both files should be at the end;
    // otherwise, one file has extra line(s) that shouldn't be there.
    REQUIRE((feof(golden_file) && feof(test_file)));
    fclose(golden_file);

    delete[] golden_buf;
    delete[] test_buf;
    return true;
}

TEST_CASE("Single-value int statistics", "singlevalue_int") {

    int value = 0;
    SECTION("Using default values for unspecified constructor params") {
        Statistic<int> mystat("test_stat", "Description of a test statistic.", &value, 1);
        CHECK(mystat.get_name().compare("test_stat") == 0);
        CHECK(mystat.get_value() == value);
        CHECK(mystat.get_init_val() == 1);
        CHECK(mystat.get_output_fmt().compare("%12d") == 0);
        // Using an initial value of 1 should set the actual stat value to 1.
        REQUIRE(value == 1);
        mystat.save_value();
        mystat.scale_value(2);
        // SCaling the value only changes the final value.
        REQUIRE(mystat.get_value() == 1);
        REQUIRE(mystat.get_final_val() == 2);
        value = 20;
        REQUIRE(mystat.get_value() == value);
    }

    SECTION("Specifying values for all constructor params.") {
        Statistic<int> mystat(
            "test_stat", "Description of a test statistic.", &value, 0, "%2d", false, false);
        CHECK(mystat.is_printed() == false);
        CHECK(mystat.is_scaled() == false);
        CHECK(mystat.get_output_fmt().compare("%2d") == 0);
    }

    SECTION("Printing single value") {
        char temp_file_name[21];
        FILE* temp_file = open_temp_file(temp_file_name);

        Statistic<int> mystat("test_stat", "Description of a test statistic.", &value, 0, "%6d");
        value = 1000;
        mystat.print_value(temp_file);

        check_printfs(temp_file, XIOSIM_PACKAGE_PATH + "test_data/test_stat.singlevalue_int.out");
        cleanup_temp_file(temp_file, temp_file_name);
    }

}

TEST_CASE("String type single-value statistics", "singlevalue_str") {
    const char* value = "something";
    Statistic<const char*> mystat("string_stat", "Description of a string statistic.", value);
    CHECK(mystat.get_name().compare("string_stat") == 0);
    CHECK(strcmp(mystat.get_value(), "something") == 0);
    mystat.scale_value(2.0);
    CHECK(strcmp(mystat.get_value(), "something") == 0);
}

TEST_CASE("Single-value double statistics", "singlevalue_double") {
    double value = 0;
    SECTION("Default output format") {
        Statistic<double> mystat("value_stat", "Description", &value, 0);
        value = 0.12349;
        CHECK(mystat.get_output_fmt().compare("%12.4f") == 0);
        CHECK(mystat.get_value() == value);
    }

    SECTION("Specifying custom output format") {
        Statistic<double> mystat("double_stat", "Description", &value, 0, "%14.4f");
        CHECK(mystat.get_output_fmt().compare("%14.4f") == 0);
    }
}

TEST_CASE("Distribution", "distribution") {
    const int array_sz = 10;
    int init_val = 0;
    SECTION("Unlabeled distribution") {
        char temp_file_name[21];
        FILE* temp_file = open_temp_file(temp_file_name);

        Distribution dist("dist", "Distribution description.", init_val, array_sz);
        dist.add_samples(0, 10);
        dist.add_samples(1, 10);
        dist.add_samples(2, 10);
        dist.add_samples(3, 10);
        dist.add_samples(4, 10);
        dist.add_samples(5, 10);
        dist.add_samples(6, 10);
        dist.add_samples(7, 10);
        dist.add_samples(8, 10);
        dist.add_samples(9, 10);
        dist.add_samples(10, 10);

        dist_stats_t stats;
        dist.compute_dist_stats(&stats);
        REQUIRE(stats.count == 100);
        REQUIRE(stats.sum == 450);
        REQUIRE(stats.mean == Approx(4.5));
        REQUIRE(stats.stddev == Approx(2.8723));
        REQUIRE(dist.get_overflows() == 10);

        dist.print_value(temp_file);

        check_printfs(temp_file, XIOSIM_PACKAGE_PATH + "test_data/test_stat.distribution.unlabeled.out");
        cleanup_temp_file(temp_file, temp_file_name);
    }

    SECTION("Printing distribution with format and labels") {
        char temp_file_name[21];
        FILE* temp_file = open_temp_file(temp_file_name);

        const char* stat_labels[array_sz] = {
            "label_1", "label_2", "label_3", "label_4", "label_5",
            "label_6", "label_7", "label_8", "label_9", "label_10"
        };
        const char* output_fmt = "%5.1f ";
        Distribution dist("labeled_dist",
                          "Distribution description",
                          init_val,
                          array_sz,
                          stat_labels,
                          output_fmt);
        dist.add_samples(0, 10);
        dist.add_samples(2, 13);
        dist.add_samples(5, 1);
        dist.add_samples(6, 20);

        dist.print_value(temp_file);

        check_printfs(temp_file, XIOSIM_PACKAGE_PATH + "test_data/test_stat.distribution.labeled.out");
        cleanup_temp_file(temp_file, temp_file_name);
    }
}

TEST_CASE("Sparse histograms", "histogram") {
    SparseHistogram histogram("histogram", "A histogram");
    histogram.add_samples(0,2);
    histogram.add_samples(0,2);
    histogram.add_samples(3,2);
    histogram.add_samples(0,2);
    histogram.add_samples(0,2);
    histogram.add_samples(1,2);
    histogram.add_samples(1,2);
    histogram.add_samples(0, 8);
    histogram.add_samples(50, 2);

    SECTION("Basic usage.") {
        char temp_file_name[21];
        FILE* temp_file = open_temp_file(temp_file_name);

        histogram.print_value(temp_file);
        check_printfs(temp_file,
                      XIOSIM_PACKAGE_PATH + "test_data/test_stat.histogram.out");
        cleanup_temp_file(temp_file, temp_file_name);
    }

    SECTION("Scaling a histogram (if it is sampled).") {
        histogram.scale_value(0.5);
        char temp_file_name[21];
        FILE* temp_file = open_temp_file(temp_file_name);

        histogram.print_value(temp_file);
        check_printfs(temp_file,
                      XIOSIM_PACKAGE_PATH + "test_data/test_stat.histogram_scaled.out");
        cleanup_temp_file(temp_file, temp_file_name);
    }
}

TEST_CASE("Formula statistics", "formulas") {
    SECTION("Formulas that add constants.") {
        Formula f("add_constants", "description");
        f = Constant(3) + 4;
        CHECK(f.evaluate() == 7);
    }

    SECTION("Formulas that add and subtract existing Statistics and constants") {
        int int_value = 0;
        Statistic<int> int_stat("integer_stat", "integer statistic", &int_value, 0);
        Formula f("add_stats_constants", "description");
        f = int_stat + 5;

        REQUIRE(f.evaluate() == 5);

        int_value = 100;
        REQUIRE(f.evaluate() == 105);

        int_value++;
        REQUIRE(f.evaluate() == 106);

        int_value--;
        REQUIRE(f.evaluate() == 105);
    }

    SECTION("Formulas that add and subtract statistics of the same type.") {
        int int_statvalue_1 = 0;
        int int_statvalue_2 = 0;
        Statistic<int> int_stat1("integer_stat1", "integer statistic", &int_statvalue_1, 0);
        Statistic<int> int_stat2("integer_stat2", "another integer statistic", &int_statvalue_2, 0);
        Formula add("add_stats_stats", "Add statistics");
        Formula sub("sub_stats_stats", "Subtract statistics");
        add = int_stat1 + int_stat2;
        sub = int_stat1 - int_stat2;

        CHECK(add.evaluate() == 0);
        CHECK(sub.evaluate() == 0);

        int_statvalue_1 = 10;
        int_statvalue_2 = -10;
        CHECK(add.evaluate() == 0);
        CHECK(sub.evaluate() == 20);

        int_statvalue_1 = 1000;
        int_statvalue_2 = -1;
        CHECK(add.evaluate() == 999);
        CHECK(sub.evaluate() == 1001);
    }

    SECTION("Formulas that add and subtract float and int statistics.") {
        float flt_statvalue_1 = 0;
        unsigned int int_statvalue_2 = 0;
        Statistic<float> flt_stat1("float_stat1", "float statistic", &flt_statvalue_1, 0);
        Statistic<unsigned int> int_stat2(
            "integer_stat2", "unsigned integer statistic", &int_statvalue_2, 0);
        Formula add("add_stats", "Add statistics");
        Formula sub("sub_stats", "Subtract statistics");
        add = flt_stat1 + int_stat2;
        sub = flt_stat1 - int_stat2;

        CHECK(add.evaluate() == 0);
        CHECK(sub.evaluate() == 0);

        flt_statvalue_1 = 0.5;
        int_statvalue_2 = 1;
        CHECK(add.evaluate() == 1.5);
        CHECK(sub.evaluate() == -0.5);

        flt_statvalue_1 = 101.2345;
        int_statvalue_2 = 1000;
        CHECK(add.evaluate() == Approx(1101.2345));
        CHECK(sub.evaluate() == Approx(-898.7655));
    }

    SECTION("Formulas with 3 operands.") {
        int stat_1_value = 0;
        unsigned stat_2_value = 0;
        int64_t stat_3_value = 0;

        Statistic<int> stat_1("stat_1", "integer statistic", &stat_1_value, 0);
        Statistic<unsigned> stat_2("stat_2", "unsigned statistic", &stat_2_value, 0);
        Statistic<int64_t> stat_3("stat_3", "int64_t statistic", &stat_3_value, 0);
        Formula sum("add_three_stats", "Add three statistics together");
        Formula nested_sum("add_three_stats_nested", "Add explicitly nested statistics together.");
        sum = stat_1 + stat_2 + stat_3;
        nested_sum = stat_1 + (stat_2 + stat_3);

        REQUIRE(sum.evaluate() == 0);

        stat_1_value = 10;
        REQUIRE(sum.evaluate() == 10);
        REQUIRE(nested_sum.evaluate() == 10);

        stat_2_value++;
        REQUIRE(sum.evaluate() == 11);
        REQUIRE(nested_sum.evaluate() == 11);

        stat_3_value += 3;
        REQUIRE(sum.evaluate() == 14);
        REQUIRE(nested_sum.evaluate() == 14);

        Formula madd("multiply-add", "Performs a*b + c");
        Formula nested_madd("nested-multiply-add", "Performs a*(b+c)");
        madd = stat_1 * stat_2 + stat_3;
        nested_madd = stat_1 * (stat_2 + stat_3);
        stat_1_value = -1;
        stat_2_value = 2;
        stat_3_value = 3;
        CHECK(madd.evaluate() == 1);
        CHECK(nested_madd.evaluate() == -5);

        stat_1_value = -1;
        stat_2_value = 1;
        stat_3_value = 5;
        CHECK(madd.evaluate() == 4);
        CHECK(nested_madd.evaluate() == -6);

        stat_1_value = 2;
        stat_2_value = 5;
        stat_3_value = 5;
        CHECK(madd.evaluate() == 15);
        CHECK(nested_madd.evaluate() == 20);
    }

    SECTION("Testing assignment operators", "assignment") {
        int stat_1_value = 0;
        unsigned stat_2_value = 0;
        Statistic<int> stat_1("stat_1", "integer statistic", &stat_1_value, 0);
        Statistic<unsigned> stat_2("stat_2", "unsigned statistic", &stat_2_value, 0);
        Formula assign("test_assignment", "Use the += operator.");

        // Start by assigning the Formula any random expression.
        assign = stat_1;
        REQUIRE(assign.evaluate() == 0);

        // Now, do an addition-assignment.
        assign += stat_2;
        REQUIRE(assign.evaluate() == 0);

        // Change some values!
        stat_2_value += 5;
        REQUIRE(assign.evaluate() == 5);
        stat_1_value += 5;
        REQUIRE(assign.evaluate() == 10);
    }

    SECTION("Nested formulas.") {
        int stat_1_value = 0;
        unsigned stat_2_value = 0;
        int64_t stat_3_value = 0;

        Statistic<int> stat_1("stat_1", "integer statistic", &stat_1_value, 0);
        Statistic<unsigned> stat_2("stat_2", "unsigned statistic", &stat_2_value, 0);
        Statistic<int64_t> stat_3("stat_3", "int64_t statistic", &stat_3_value, 0);

        Formula sum("add_three_stats", "Add three statistics together");
        Formula partial_sum("two_out_of_three", "Partial sum of the above");
        Formula nested_sum("add_three_stats_nested", "Add explicitly nested statistics together.");
        sum = stat_1 + stat_2 + stat_3;
        partial_sum = stat_2 + stat_3;
        nested_sum = stat_1 + partial_sum;

        REQUIRE(sum.evaluate() == 0);
        REQUIRE(nested_sum.evaluate() == 0);

        stat_1_value = 10;
        REQUIRE(sum.evaluate() == 10);
        REQUIRE(nested_sum.evaluate() == 10);

        stat_2_value++;
        REQUIRE(sum.evaluate() == 11);
        REQUIRE(nested_sum.evaluate() == 11);

        stat_3_value += 3;
        REQUIRE(sum.evaluate() == 14);
        REQUIRE(nested_sum.evaluate() == 14);

        SECTION("Nested formula copying.") {
            /* Modifying partial_sum shouldn't affect nested_sum. */
            partial_sum += stat_3; /* == stat_2 + 2 * stat_3 */
            REQUIRE(nested_sum.evaluate() == 14);
            REQUIRE(partial_sum.evaluate() == 7);
        }
    }

    SECTION("More nested formulas.") {
        int inst_1_val;
        int cycles_1_val;
        Statistic<int> inst_1("inst_1", "", &inst_1_val, 0);
        Statistic<int> cycles_1("inst_1", "", &cycles_1_val, 0);
        inst_1_val = 112124;
        cycles_1_val = 222843;

        int inst_2_val;
        int cycles_2_val;
        Statistic<int> inst_2("inst_2", "", &inst_2_val, 0);
        Statistic<int> cycles_2("inst_2", "", &cycles_2_val, 0);
        inst_2_val = 112124;
        cycles_2_val = 219149;

        Formula ipc_1("IPC_1", "IPC");
        ipc_1 = inst_1 / cycles_1;
        REQUIRE(ipc_1.evaluate() == Approx(0.50315));

        Formula ipc_2("IPC_2", "IPC");
        ipc_2 = inst_2 / cycles_2;
        REQUIRE(ipc_2.evaluate() == Approx(0.51163));

        Formula g_IPC("g_IPC", "geomean IPC");
        g_IPC *= ipc_1;
        g_IPC *= ipc_2;
        g_IPC ^= 0.5;
        REQUIRE(g_IPC.evaluate() == Approx(0.50738));
    }
}


TEST_CASE("Registering formula statistics through the API layer.", "stat_reg_formula") {
    char temp_file_name[21];
    FILE* temp_file = open_temp_file(temp_file_name);

    StatsDatabase sdb;
    int int_value = 0;
    double double_value = 0.0;
    auto int_stat = stat_reg_int(
            &sdb, true, "integer_stat", "integer description", &int_value, 0, false, NULL);
    auto double_stat = stat_reg_double(
            &sdb, true, "double_stat", "double description", &double_value, 0, false, NULL);
    Formula* formula =
            stat_reg_formula(&sdb, true, "formula", "A formula", int_stat + double_stat, NULL);
    Formula* ratio = stat_reg_formula(
            &sdb, true, "ratio", "A ratio", int_stat / (double_stat * 1000.0), NULL);
    REQUIRE(formula->evaluate() == 0);

    int_value = 5;
    double_value = 0.5;
    REQUIRE(formula->evaluate() == 5.5);
    REQUIRE(ratio->evaluate() == Approx(0.01));

    int_value = 1;
    REQUIRE(formula->evaluate() == 1.5);
    REQUIRE(ratio->evaluate() == Approx(0.002));

    double_value = 7.25;
    REQUIRE(formula->evaluate() == 8.25);
    REQUIRE(ratio->evaluate() == Approx(0.00013793));

    sdb.print_all_stats(temp_file);
    check_printfs(temp_file, XIOSIM_PACKAGE_PATH + "test_data/test_stat.reg_api.out");
    cleanup_temp_file(temp_file, temp_file_name);
}


TEST_CASE("Full statistics database", "database") {
    char temp_file_name[21];
    FILE* temp_file = open_temp_file(temp_file_name);

    int int_value = 0;
    double double_value = 0.0;
    int64_t ll_value = 0;
    const char* str_value = "string";

    StatsDatabase sdb;
    Statistic<int>* int_stat =
        sdb.add_statistic("integer_stat", "integer description", &int_value, 0);
    Statistic<double>* double_stat =
        sdb.add_statistic("double_stat", "double description", &double_value, 0);
    Statistic<int64_t>* ll_stat =
        sdb.add_statistic("long_long_stat", "int64_t description", &ll_value, 0);
    sdb.add_statistic("string_stat", "string description", str_value);
    Distribution* dist = sdb.add_distribution("dist", "dist description", 0, 4);
    SparseHistogram* hist = sdb.add_sparse_histogram("hist", "hist description");

    // Don't need to add too many; distributions/histograms have their own unit tests.
    dist->add_samples(0, 50);
    dist->add_samples(50, 1);
    hist->add_samples(0, 5);
    hist->add_samples(10, 7);

    // Modify the stat values.
    int_value = 100;
    double_value = 50.123;
    ll_value = 1233445;

    CHECK(int_stat->get_value() == int_value);
    CHECK(double_stat->get_value() == double_value);
    CHECK(ll_stat->get_value() == ll_value);

    sdb.print_all_stats(temp_file);

    check_printfs(temp_file, XIOSIM_PACKAGE_PATH + "test_data/test_stat.database.out");
    cleanup_temp_file(temp_file, temp_file_name);
}
