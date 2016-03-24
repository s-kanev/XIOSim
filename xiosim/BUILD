# Description:
#   XIOSim simulation library: libsim.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # BSD

exports_files(["LICENSE"])

test_suite(
    name = "unit_tests",
    tags = ["small"],
)

cc_library(
    name = "libsim",
    hdrs = ["libsim.h"],
    deps = [
        ":memory",
        ":sim",
        ":zesto-cache",
        ":zesto-core",
        ":zesto-dram",
        ":zesto-dvfs",
        ":zesto-power",
        ":zesto-uncore",
    ],
)

filegroup(
    name = "configs",
    srcs = glob(["config/*.cfg"]),
)

cc_library(
    name = "zesto-uncore",
    srcs = ["zesto-uncore.cpp"],
    hdrs = ["zesto-uncore.h"],
    deps = [
        ":stats",
        ":zesto-MC",
        ":zesto-cache",
        ":zesto-noc",
        ":zesto-repeater",
    ],
)

cc_library(
    name = "zesto-noc",
    srcs = ["zesto-noc.cpp"],
    hdrs = ["zesto-noc.h"],
    deps = [
        ":misc",
        ":stats",
    ],
)

cc_library(
    name = "zesto-cache",
    srcs = [
        "2bitc.h",
        "zesto-cache.cpp",
    ],
    hdrs = ["zesto-cache.h"],
    deps = [
        ":stats",
        ":zesto-coherence",
        ":zesto-prefetch",
    ],
)

load("components", "gen_list")

gen_list(
    component = "fetch",
    dirs = ["ZPIPE-fetch"],
)

gen_list(
    component = "decode",
    dirs = ["ZPIPE-decode"],
)

gen_list(
    component = "alloc",
    dirs = ["ZPIPE-alloc"],
)

gen_list(
    component = "exec",
    dirs = ["ZPIPE-exec"],
)

gen_list(
    component = "commit",
    dirs = ["ZPIPE-commit"],
)

gen_list(
    component = "bpred",
    dirs = [
        "ZCOMPS-bpred",
        "ZCOMPS-fusion",
        "ZCOMPS-btb",
        "ZCOMPS-ras",
    ],
)

gen_list(
    component = "memdep",
    dirs = ["ZCOMPS-memdep"],
)

gen_list(
    component = "prefetch",
    dirs = ["ZCOMPS-prefetch"],
)

gen_list(
    component = "coherence",
    dirs = ["ZCOMPS-coherence"],
)

gen_list(
    component = "dram",
    dirs = ["ZCOMPS-dram"],  #, extra_deps=[":zesto-uncore"])
)

gen_list(
    component = "MC",
    dirs = ["ZCOMPS-MC"],
)

gen_list(
    component = "repeater",
    dirs = ["ZCOMPS-repeater"],
)

gen_list(
    component = "power",
    dirs = ["ZCORE-power"],
    extra_deps = [
        "//third_party/mcpat:lib",
        ":zesto-core",
        ":zesto-uncore",
    ],
)

gen_list(
    component = "dvfs",
    dirs = ["ZCOMPS-dvfs"],
)

cc_library(
    name = "zesto-core",
    srcs = [
        "2bitc.h",
        "zesto-core.cpp",
        "zesto-oracle.cpp",
    ],
    hdrs = [
        "zesto-core.h",
        "zesto-oracle.h",
    ],
    deps = [
        ":shadow_MopQ",
        ":stats",
        ":synchronization",
        ":zesto-alloc",
        ":zesto-bpred",
        ":zesto-commit",
        ":zesto-decode",
        ":zesto-exec",
        ":zesto-fetch",
        ":zesto-memdep",
        ":zesto-structs",
        ":ztrace",
    ],
)

cc_library(
    name = "shadow_MopQ",
    srcs = ["shadow_MopQ.cpp"],
    hdrs = ["shadow_MopQ.h"],
    deps = [
        ":misc",
        ":zesto-structs",
        "//xiosim/pintool:buffer",
        "//xiosim/pintool:handshake_container",
    ],
)

cc_library(
    name = "sim",
    srcs = [
        "sim.cpp",
        "sim-loop.cpp",
        "slices.cpp",
    ],
    hdrs = [
        "sim.h",
        "sim-loop.h",
        "slices.h",
    ],
    deps = [
        ":memory",
        ":stats",
        ":synchronization",
        ":zesto-core",
        ":zesto-structs",
        ":zesto-uncore",
        ":ztrace",
    ],
)

cc_library(
    name = "zesto-config",
    srcs = [
        "zesto-config.cpp",
        "zesto-config-params.cpp",
    ],
    hdrs = ["zesto-config.h"],
    deps = [
        ":knobs",
        "//third_party/confuse",
    ],
)

cc_test(
    name = "test_parse_configs",
    size = "small",
    srcs = ["test_parse_configs.cpp"],
    data = ["config/default.cfg"],
    linkopts = ["-lm"],
    deps = [
        ":catch_impl",
        ":zesto-config",
        "//third_party/catch:main",
        "//third_party/confuse",
    ],
)

cc_library(
    name = "memory",
    srcs = ["memory.cpp"],
    hdrs = ["memory.h"],
    deps = [
        ":misc",
        ":stats",
        ":synchronization",
        ":ztrace",
    ],
)

cc_library(
    name = "synchronization",
    hdrs = ["synchronization.h"],
)

cc_library(
    name = "ztrace",
    srcs = ["ztrace.cpp"],
    hdrs = ["ztrace.h"],
    deps = [
        ":misc",
        ":stats",
        ":x86",
        ":zesto-structs",
    ],
)

cc_library(
    name = "knobs",
    hdrs = ["knobs.h"],
    deps = [":x86"],
)

cc_library(
    name = "zesto-structs",
    hdrs = ["zesto-structs.h"],
    defines = ["USE_SSE_MOVE"],
    deps = [":x86"],
)

cc_library(
    name = "x86",
    srcs = [
        "decode.cpp",
        "fu.cpp",
        "uop_cracker.cpp",
    ],
    hdrs = [
        "decode.h",
        "fu.h",
        "regs.h",
        "uop_cracker.h",
    ],
    deps = [
        ":misc",
        ":stats",
        "//third_party/pin:xed",
    ],
)

cc_test(
    name = "test_decoder",
    size = "small",
    srcs = [
        "test_decoder.cpp",
        "test_xed_context.h",
    ],
    deps = [
        ":catch_impl",
        ":misc",
        ":x86",
        ":zesto-structs",
        "//third_party/catch:main",
        "//third_party/pin:xed",
    ],
)

cc_test(
    name = "test_uop_cracker",
    size = "small",
    srcs = [
        "test_uop_cracker.cpp",
        "test_xed_context.h",
    ],
    deps = [
        ":catch_impl",
        ":misc",
        ":x86",
        ":zesto-structs",
        "//third_party/catch:main",
        "//third_party/pin:xed",
    ],
)

cc_library(
    name = "misc",
    srcs = ["misc.cpp"],
    hdrs = [
        "host.h",
        "misc.h",
    ],
    copts = ["-DUSE_SSE_MOVE"],
)

cc_library(
    name = "stats",
    srcs = [
        "expression.h",
        "expression_impl.cpp",
        "expression_impl.h",
        "stat_database.h",
        "statistic.h",
        "stats.cpp",
    ],
    hdrs = [
        "host.h",
        "stats.h",
    ],
    deps = [
        ":boost_stats_include",
    ],
)

cc_test(
    name = "test_stat_database",
    size = "small",
    srcs = ["test_stat_database.cpp"],
    data = glob(["test_data/test_stat.*.out"]),
    linkopts = ["-lm"],
    deps = [
        ":catch_impl",
        ":stats",
        "//third_party/catch:main",
    ],
)

cc_library(
    name = "boost_stats_include",
    hdrs = ["boost_statistics.h"],
    deps = [
        "//third_party/boost:accumulators",
    ],
)

# compile catch runner code only once
cc_library(
    name = "catch_impl",
    srcs = ["catch_impl.cpp"],
    deps = ["//third_party/catch:main"],
)
