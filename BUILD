package( default_visibility = ["//visibility:public"] )

cc_library(
    name = "libsim",
    hdrs = [ "libsim.h" ],
    deps = [
        ":sim",
        ":memory",
        ":zesto-cache",
        ":zesto-core",
        ":zesto-dvfs",
        ":zesto-dram",
        ":zesto-uncore",
        ":zesto-power",
    ],
)

cc_library(
    name = "zesto-uncore",
    hdrs = [ "zesto-uncore.h" ],
    srcs = [ "zesto-uncore.cpp" ],
    deps = [
        ":stats",
        ":zesto-cache",
        ":zesto-MC",
        ":zesto-noc",
        ":zesto-repeater",
    ],
)

cc_library(
    name = "zesto-noc",
    hdrs = [ "zesto-noc.h" ],
    srcs = [ "zesto-noc.cpp" ],
    deps = [
        ":misc",
        ":stats",
    ],
)

cc_library(
    name = "zesto-cache",
    hdrs = [ "zesto-cache.h" ],
    srcs = [
        "zesto-cache.cpp",
        "2bitc.h",
     ],
    deps = [
        ":zesto-prefetch",
        ":zesto-coherence",
        ":stats",
    ],
)

load("components", "gen_list")
gen_list(component = "fetch", dirs = ["ZPIPE-fetch"])
gen_list(component = "decode", dirs = ["ZPIPE-decode"])
gen_list(component = "alloc", dirs = ["ZPIPE-alloc"])
gen_list(component = "exec", dirs = ["ZPIPE-exec"])
gen_list(component = "commit", dirs = ["ZPIPE-commit"])
gen_list(component = "bpred", dirs = ["ZCOMPS-bpred", "ZCOMPS-fusion", "ZCOMPS-btb", "ZCOMPS-ras"])
gen_list(component = "memdep", dirs = ["ZCOMPS-memdep"])
gen_list(component = "prefetch", dirs = ["ZCOMPS-prefetch"])
gen_list(component = "coherence", dirs = ["ZCOMPS-coherence"])

gen_list(component = "dram", dirs = ["ZCOMPS-dram"])#, extra_deps=[":zesto-uncore"])
gen_list(component = "MC", dirs = ["ZCOMPS-MC"])
gen_list(component = "repeater", dirs = ["ZCOMPS-repeater"])

gen_list(component = "power", dirs = ["ZCORE-power"], extra_deps=["//mcpat:lib", ":zesto-core", ":zesto-uncore"])
gen_list(component = "dvfs", dirs = ["ZCOMPS-dvfs"])

cc_library(
    name = "zesto-core",
    hdrs = [
        "zesto-core.h",
        "zesto-oracle.h",
    ],
    srcs = [
        "zesto-core.cpp",
        "zesto-oracle.cpp",
        "2bitc.h",
    ],
    deps = [
        ":shadow_MopQ",
        ":stats",
        ":synchronization",
        ":zesto-structs",
        ":ztrace",
        ":zesto-fetch",
        ":zesto-decode",
        ":zesto-alloc",
        ":zesto-exec",
        ":zesto-commit",
        ":zesto-bpred",
        ":zesto-memdep",
    ],
)

cc_library(
    name = "shadow_MopQ",
    hdrs = [ "shadow_MopQ.h" ],
    srcs = [ "shadow_MopQ.cpp" ],
    deps = [
        ":misc",
        "//pintool:buffer",
        "//pintool:handshake_container",
        ":zesto-structs",
    ],
)

cc_library(
    name = "sim",
    hdrs = [
        "sim.h",
        "sim-loop.h",
        "slices.h",
    ],
    srcs = [
        "sim.cpp",
        "sim-loop.cpp",
        "slices.cpp",
    ],
    deps = [
        ":memory",
        ":stats",
        ":synchronization",
        ":zesto-config",
        ":zesto-structs",
        ":zesto-core",
        ":zesto-uncore",
        ":ztrace",
    ],
)

cc_library(
    name = "zesto-config",
    hdrs = [ "zesto-config.h" ],
    srcs = [
        "zesto-config.cpp",
        "zesto-config-params.cpp",
    ],
    deps = [
        ":zesto-structs",
        "//pintool:ezOptionParser",
        "@confuse//:main",
    ],
)

cc_library(
    name = "memory",
    hdrs = [ "memory.h" ],
    srcs = [ "memory.cpp" ],
    deps = [
        ":misc",
        ":stats",
        ":synchronization",
        ":ztrace",
    ],
)

cc_library(
    name = "synchronization",
    hdrs = [ "synchronization.h" ],
)

cc_library(
    name = "ztrace",
    hdrs = [ "ztrace.h" ],
    srcs = [ "ztrace.cpp" ],
    deps = [
        ":misc",
        ":stats",
        ":x86",
        ":zesto-structs",
    ],
)

cc_library(
    name = "zesto-structs",
    hdrs = [ "zesto-structs.h" ],
    defines = [ "USE_SSE_MOVE" ],
    deps = [ ":x86" ],
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
        "uop_cracker.h",
        "regs.h",
    ],
    deps = [
        ":misc",
        ":stats",
        "@pin//:xed"
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
        "expression.cpp",
        "expression.h",
        "stats.cpp",
        "statistic.h",
        "stat_database.h",
    ],
    hdrs = [
        "host.h",
        "stats.h",
    ],
    deps = [
        ":boost_stats_include",
    ],
)

cc_library(
    name = "boost_stats_include",
    hdrs = ["boost_statistics.h"],
    deps = [
        "@boost//:accumulators",
    ],
)
