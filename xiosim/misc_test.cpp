#include "catch.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "misc.h"

const std::string XIOSIM_PACKAGE_PATH = "xiosim/";

TEST_CASE("gzopen", "gz read") {
    std::string path = XIOSIM_PACKAGE_PATH + "test_data/gzopen.gz";
    FILE* fd = gzopen(path.c_str(), "r");
    REQUIRE(fd != NULL);
    char buff[255];
    char* res = fgets(buff, sizeof(buff), fd);
    REQUIRE(res != NULL);
    REQUIRE(strncmp(buff, "0xdecafbad\n", sizeof(buff)) == 0);
    gzclose(fd);
}

TEST_CASE("gz overflow", "gz overflow") {
    const size_t size = 4 * 1024 * 1024;
    char* fname = static_cast<char*>(malloc(size));
    memset(fname, 'a', size);
    memcpy(fname + size - 4, ".gz", 3);
    fname[size - 1] = 0;
    FILE* fd = gzopen(fname, "r");
    REQUIRE(fd == NULL);
    free(fname);
}
