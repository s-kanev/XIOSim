/*
 * Tests some basic functions of the size class cache.
 */

#include <iostream>
#include "catch.hpp"
#include "size_class_cache.h"

TEST_CASE("Basic size class lookups", "size class") {
    SizeClassCache cache(5);
    cache_entry_t result;

    SECTION("Empty cache.") {
        // Empty cache.
        REQUIRE(cache.size_lookup(32, result) == false);
        REQUIRE(cache.size_lookup(1, result) == false);
        REQUIRE(cache.size_lookup(16, result) == false);
        REQUIRE(cache.size_lookup(200, result) == false);
    }

    // Insert a few values (but don't fill up the cache).
    REQUIRE(cache.size_update(24, 32, 3) == true);
    REQUIRE(cache.size_update(1, 8, 1) == true);
    REQUIRE(cache.size_update(568, 576, 26) == true);
    REQUIRE(cache.size_update(48, 48, 4) == true);

    SECTION("Search for exact sizes added.") {
        REQUIRE(cache.size_lookup(24, result) == true);
        REQUIRE(result.get_size_class() == 3);
        REQUIRE(result.get_size() == 32);
        REQUIRE(cache.size_lookup(1, result) == true);
        REQUIRE(result.get_size_class() == 1);
        REQUIRE(result.get_size() == 8);
        REQUIRE(cache.size_lookup(568, result) == true);
        REQUIRE(result.get_size_class() == 26);
        REQUIRE(result.get_size() == 576);
    }

    SECTION("Search for sizes that should have the same class index.") {
        REQUIRE(cache.size_lookup(7, result) == true);
        REQUIRE(result.get_size_class() == 1);
        REQUIRE(result.get_size() == 8);
        REQUIRE(cache.size_lookup(32, result) == true);
        REQUIRE(result.get_size_class() == 3);
        REQUIRE(result.get_size() == 32);
        REQUIRE(cache.size_lookup(567, result) == true);
        REQUIRE(result.get_size_class() == 26);
        REQUIRE(result.get_size() == 576);
    }

    SECTION("Search for sizes that do not match on the class index.") {
        REQUIRE(cache.size_lookup(552, result) == false);
        REQUIRE(cache.size_lookup(560, result) == false);
    }
}

TEST_CASE("Expansion of index ranges", "expansion") {
    SizeClassCache cache(5);
    cache_entry_t result;

    SECTION("Should expand range from (4,5] to (3,5].") {
        cache.size_update(32, 32, 3);
        REQUIRE(cache.size_lookup(24, result) == false);
        REQUIRE(cache.size_update(24, 32, 3) == true);
    }

    SECTION("Should leave range untouched.") {
        cache.size_update(513, 576, 26);
        REQUIRE(cache.size_update(530, 576, 26) == false);
        REQUIRE(cache.size_update(575, 576, 26) == false);
    }
}
