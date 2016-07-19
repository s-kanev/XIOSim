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
        ":core_const",
        ":misc",
        ":stats",
        ":uarch_headers",
    ],
)

cc_library(
    name = "zesto-cache",
    srcs = ["zesto-cache.cpp"],
    hdrs = ["zesto-cache.h"],
    deps = [
        ":memory",
        ":stats",
        ":zesto-coherence",
        ":zesto-prefetch",
        ":ztrace",
    ],
)

load("components", "gen_list")

gen_list(
    component = "fetch",
    dirs = ["ZPIPE-fetch"],
    extra_deps = [
        ":memory",
        ":ztrace",
    ],
)

gen_list(
    component = "decode",
    dirs = ["ZPIPE-decode"],
    extra_deps = [":ztrace"],
)

gen_list(
    component = "alloc",
    dirs = ["ZPIPE-alloc"],
    extra_deps = [
        ":helix",
        ":ztrace",
    ],
)

gen_list(
    component = "exec",
    dirs = ["ZPIPE-exec"],
    extra_deps = [
        ":helix",
        ":memory",
        ":zesto-memdep",
        ":ztrace",
    ],
)

gen_list(
    component = "commit",
    dirs = ["ZPIPE-commit"],
    extra_deps = [":ztrace"],
)

gen_list(
    component = "bpred",
    dirs = [
        "ZCOMPS-bpred",
        "ZCOMPS-fusion",
        "ZCOMPS-btb",
        "ZCOMPS-ras",
    ],
    extra_deps = [
        ":2bitc",
        ":valcheck",
    ],
)

gen_list(
    component = "memdep",
    dirs = ["ZCOMPS-memdep"],
    extra_deps = [":valcheck"],
)

gen_list(
    component = "prefetch",
    dirs = ["ZCOMPS-prefetch"],
    extra_deps = [
        ":2bitc",
        ":memory",
        ":valcheck",
    ],
)

gen_list(
    component = "coherence",
    dirs = ["ZCOMPS-coherence"],
    extra_deps = [
        ":memory",
        ":synchronization"
    ],
)

gen_list(
    component = "dram",
    dirs = ["ZCOMPS-dram"],
)

gen_list(
    component = "MC",
    dirs = ["ZCOMPS-MC"],
    extra_deps = [":memory"],
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
    extra_srcs = [
        "sim.h" # for cores
    ],
)

gen_list(
    component = "dvfs",
    dirs = ["ZCOMPS-dvfs"],
    extra_deps = [
        ":ztrace",
    ],
)

cc_library(
    name = "zesto-core",
    srcs = [
        "zesto-core.cpp",
        "zesto-oracle.cpp",
    ],
    hdrs = [
        "zesto-core.h",
        "zesto-oracle.h",
    ],
    deps = [
        ":memory",
        ":shadow_MopQ",
        ":stats",
        ":synchronization",
        ":uarch_headers",
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
        ":uarch_headers",
        ":zesto-structs",
        "//xiosim/pintool:buffer",
        "//xiosim/pintool:handshake_container",
    ],
)

cc_library(
    name = "sim",
    srcs = [
        "libsim.h",
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
        ":knobs",
        ":memory",
        ":stats",
        ":synchronization",
        ":zesto-core",
        ":zesto-power",
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
        "core_const",
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
        ":core_const",
        ":misc",
        ":stats",
        ":uarch_headers",
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
    deps = [
        ":host",
        ":x86",
    ],
)

cc_library(
    name = "x86",
    srcs = [
        "decode.cpp",
        "fu.cpp",
        "uop_cracker.cpp",
        "zesto-structs.h", # to avoid circular dependecy
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
    hdrs = ["misc.h"],
    copts = ["-DUSE_SSE_MOVE"],
    deps = [
        ":core_const",
        ":host",
    ],
)

# we don't depend on the misc rule so we can add the extra gzip flag
cc_test(
    name = "misc_test",
    size = "small",
    srcs = [
        "misc.cpp",
        "misc.h",
        "misc_test.cpp",
    ],
    copts = ['-DGZIP_PATH=\\"/bin/gzip\\"'],
    data = ["test_data/gzopen.gz"],
    deps = [
        ":core_const",
        ":catch_impl",
        ":host",
    ],
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
    hdrs = ["stats.h"],
    deps = [
        ":boost_stats_include",
        ":core_const",
        ":host",
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
    copts = ["-DCATCH_CONFIG_MAIN"],
)

cc_library(
    name = "helix",
    hdrs = ["helix.h"],
    deps = [":zesto-structs"],
)

cc_library(
    name = "2bitc",
    hdrs = ["2bitc.h"],
)

cc_library(
    name = "core_const",
    hdrs = ["core_const.h"],
)

cc_library(
    name = "core_set",
    hdrs = ["core-set.h"],
)

cc_library(
    name = "host",
    hdrs = ["host.h"],
)

cc_library(
    name = "valcheck",
    hdrs = ["valcheck.h"],
)

# There are quite a few header dependcy cycles in uarch components.
# So, we just have one large bundle of all headers and will rebuild if
# any of them change.
cc_library(
    name = "uarch_headers",
    hdrs = [
        "shadow_MopQ.h",
        "zesto-cache.h",
        "zesto-core.h",
        "zesto-dvfs.h",
        "zesto-noc.h",
        "zesto-oracle.h",
        "zesto-repeater.h",
        "zesto-fetch.h",
        "zesto-alloc.h",
        "zesto-decode.h",
        "zesto-exec.h",
        "zesto-commit.h",
        "zesto-uncore.h",
        "zesto-coherence.h",
        "zesto-bpred.h",
        "zesto-MC.h",
        "zesto-dram.h",
        "zesto-power.h",
    ],
    deps = [
        ":knobs",
        ":stats",
        ":synchronization",
        "//xiosim/pintool:buffer",
        "//xiosim/pintool:handshake_container",
    ],
)
