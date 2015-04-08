/* Unit tests for the stats database replacement.
 *
 * Author: Sam Xi
 */

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include <cstdio>
#include <cstring>

#include "stat_database.h"

TEST_CASE("Single-value int statistics", "singlevalue_int") {
    int value = 0;
    SECTION("Using default values for unspecified constructor params") {
        Statistic<int> mystat("test_stat", "Description of a test statistic.", &value, 1);
        REQUIRE(mystat.get_name().compare("test_stat") == 0);
        REQUIRE(mystat.get_value() == value);
        REQUIRE(mystat.get_init_val() == 1);
        REQUIRE(mystat.get_output_fmt().compare("%12d") == 0);
        REQUIRE(value == 1);
        mystat.scale_value(2);
        REQUIRE(mystat.get_value() == 2);
    }

    SECTION("Specifying values for all constructor params.") {
        Statistic<int> mystat(
            "test_stat", "Description of a test statistic.", &value, 0, "%2d", false, false);
        REQUIRE(mystat.is_printed() == false);
        REQUIRE(mystat.is_scaled() == false);
        REQUIRE(mystat.get_output_fmt().compare("%2d") == 0);
    }

    SECTION("Printing single value") {
        Statistic<int> mystat("test_stat", "Description of a test statistic.", &value, 0, "%6d");
        value = 1000;
        printf("------------- Print description test ---------------------\n");
        mystat.print_value(stdout);
        printf("VERIFY MANUALLY that the statistic is printed correctly.\n");
        printf("------------- Print description end ----------------------\n");
    }
}

TEST_CASE("String type single-value statistics", "singlevalue_str") {
    const char* value = "something";
    Statistic<const char*> mystat("string_stat", "Description of a string statistic.", &value);
    REQUIRE(mystat.get_name().compare("string_stat") == 0);
    REQUIRE(strcmp(mystat.get_value(), "something") == 0);
    mystat.scale_value(2.0);
    REQUIRE(strcmp(mystat.get_value(), "something") == 0);
}

TEST_CASE("Single-value double statistics", "singlevalue_double") {
    double value = 0;
    SECTION("Default output format") {
        Statistic<double> mystat("value_stat", "Description", &value, 0);
        value = 0.12349;
        REQUIRE(mystat.get_output_fmt().compare("%12.4f") == 0);
        REQUIRE(mystat.get_value() == value);
    }

    SECTION("Specifying custom output format") {
        Statistic<double> mystat("double_stat", "Description", &value, 0, "%14.4f");
        REQUIRE(mystat.get_output_fmt().compare("%14.4f") == 0);
    }
}

TEST_CASE("Distribution", "distribution") {
    const int array_sz = 10;
    const int bucket_sz = 5;
    int init_val = 0;
    SECTION("Unlabeled distribution") {
        Distribution dist("dist", "Distribution description.", init_val, array_sz, bucket_sz);
        dist.add_samples(0, 10);
        dist.add_samples(5, 10);
        dist.add_samples(10, 10);
        dist.add_samples(15, 10);
        dist.add_samples(20, 10);
        dist.add_samples(25, 10);
        dist.add_samples(30, 10);
        dist.add_samples(35, 10);
        dist.add_samples(40, 10);
        dist.add_samples(45, 10);
        dist.add_samples(50, 10);

        dist_stats_t stats;
        dist.compute_dist_stats(&stats);
        REQUIRE(stats.count == 10);
        REQUIRE(stats.sum == 100);
        REQUIRE(stats.min == 10);
        REQUIRE(stats.max == 10);
        REQUIRE(stats.mean == 10);
        REQUIRE(stats.variance == 100);
        REQUIRE(stats.stddev == 10);
        REQUIRE(dist.get_overflows() == 10);

        printf("\n------------- Print description test ---------------------\n");
        dist.print_value(stdout);
        printf("VERIFY MANUALLY that the statistic is printed correctly.\n");
        printf("------------- Print description end ----------------------\n");
    }

    SECTION("Printing distribution with format and labels") {
        const char* stat_labels[array_sz] = {
            "label_1", "label_2", "label_3", "label_4", "label_5",
            "label_6", "label_7", "label_8", "label_9", "label_10"
        };
        const char* output_fmt = "%5.1f ";
        Distribution dist("labeled_dist",
                          "Distribution description",
                          init_val,
                          array_sz,
                          bucket_sz,
                          stat_labels,
                          output_fmt);
        dist.add_samples(0, 5);
        dist.add_samples(1, 5);
        dist.add_samples(12, 13);
        dist.add_samples(27, 1);
        dist.add_samples(30, 20);

        printf("\n------------- Print description test ---------------------\n");
        dist.print_value(stdout);
        printf("VERIFY MANUALLY that the statistic is printed correctly.\n");
        printf("------------- Print description end ----------------------\n");
    }
}

TEST_CASE("Full statistics database", "database") {
    int int_value = 0;
    double double_value = 0.0;
    long long ll_value = 0;
    const char* str_value = "string";

    StatsDatabase sdb;
    Statistic<int>* int_stat =
        sdb.add_statistic("integer_stat", "integer description", &int_value, 0);
    Statistic<double>* double_stat =
        sdb.add_statistic("double_stat", "double description", &double_value, 0);
    Statistic<long long>* ll_stat =
        sdb.add_statistic("long_long_stat", "long long description", &ll_value, 0);
    sdb.add_statistic("string_stat", "string description", &str_value);
    Distribution* dist = sdb.add_distribution("dist", "dist description", 0, 10, 5);

    // Don't need to add too many; distributions have their own unit tests.
    dist->add_samples(0, 50);
    dist->add_samples(50, 1);

    // Modify the stat values.
    int_value = 100;
    double_value = 50.123;
    ll_value = 1233445;

    REQUIRE(int_stat->get_value() == int_value);
    REQUIRE(double_stat->get_value() == double_value);
    REQUIRE(ll_stat->get_value() == ll_value);

    printf("\n------------- Print description test ---------------------\n");
    sdb.print_all_stats(stdout);
    printf("VERIFY MANUALLY that the database is printed correctly.\n");
    printf("------------- Print description end ----------------------\n");
}
