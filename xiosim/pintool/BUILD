# Description:
#   XIOSim sources that don't belong in libsim.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  #BSD

exports_files(["LICENSE"])

test_suite(
    name = "unit_tests",
    tags = ["small"],
)

cc_binary(
    name = "timing_sim",
    srcs = [
        "timing_sim.cpp",
        "timing_sim.h",
    ],
    linkopts = ["-lrt"],
    deps = [
        ":allocators",
        ":buffer_manager_consumer",
        ":multiprocess",
        ":scheduler",
        "//third_party/confuse",
        "//third_party/ezOptionParser",
        "//xiosim:libsim",
        "//xiosim:zesto-config",
    ],
)

cc_binary(
    name = "feeder_zesto.so",
    srcs = [
        "feeder.h",
        "feeder_zesto.cpp",
        "ignore_ins.cpp",
        "ignore_ins.h",
        "ildjit.cpp",
        "ildjit.h",
        "paravirt.cpp",
        "paravirt.h",
        "replace_function.cpp",
        "replace_function.h",
        "roi.cpp",
        "roi.h",
        "speculation.cpp",
        "speculation.h",
        "sync_pthreads.cpp",
        "sync_pthreads.h",
        "syscall_handling.cpp",
        "syscall_handling.h",
        "utils.cpp",
        "utils.h",
        "vdso.cpp",
        "vdso.h",
    ],
    linkopts = [
        "-Wl,-Bsymbolic",
        "-lrt",
        "-lm",
        "-ldl",
        "-pthread",
    ],
    linkshared = 1,
    deps = [
        ":buffer_manager_producer",
        ":linreg",
        ":multiprocess",
        ":parse_speedup",
        "//third_party/InstLib:legacy_instlib",
        "//third_party/pin",
        "//third_party/pin:xed",
        "//xiosim:zesto-bpred",
        "//xiosim:zesto-config",
    ],
)

cc_binary(
    name = "harness",
    srcs = ["harness.cpp"],
    linkopts = [
        "-lrt",
        "-lm",
        "-pthread",
    ],
    deps = [
        ":multiprocess",
        "//third_party/confuse",
        "//third_party/ezOptionParser",
    ],
)

cc_library(
    name = "multiprocess",
    srcs = [
        "boost_interprocess.h",
        "ipc_queues.cpp",
        "ipc_queues.h",
        "mpkeys.h",
        "mpkeys_impl.cpp",
        "multiprocess_shared.cpp",
        "shared_common.h",
        "shared_map.h",
        "shared_unordered_common.h",
        "shared_unordered_map.h",
    ],
    hdrs = ["multiprocess_shared.h"],
    deps = [
        "//third_party/boost:interprocess",
        "//xiosim:misc",
        "//xiosim:synchronization",
    ],
)

cc_library(
    name = "allocators",
    srcs = [
        "base_allocator.cpp",
        "base_allocator.h",
        "gang_allocator.cpp",
        "local_opt_allocator.cpp",
        "optimization_targets.cpp",
        "penalty_allocator.cpp",
    ],
    hdrs = [
        "allocators_impl.h",
    ],
    deps = [
        ":multiprocess",
        ":speedup_models",
        "//xiosim:synchronization",
    ],
)

cc_test(
    name = "test_allocation",
    size = "small",
    srcs = ["test_allocation.cpp"],
    data = ["test_data/loop_speedup_data.csv"],
    linkopts = [
        "-lrt",
        "-lm",
        "-ldl",
        "-pthread",
    ],
    deps = [
        ":allocators",
        ":parse_speedup",
        "//third_party/catch:main",
    ],
)

cc_library(
    name = "parse_speedup",
    srcs = ["parse_speedup.cpp"],
    hdrs = ["parse_speedup.h"],
    deps = ["//third_party/boost:tokenizer"],
)

cc_test(
    name = "test_parse_speedup",
    size = "small",
    srcs = ["test_parse_speedup.cpp"],
    data = ["test_data/loop_speedup_data.csv"],
    deps = [
        ":parse_speedup",
        "//third_party/catch:main",
        "//xiosim:catch_impl",
    ],
)

cc_library(
    name = "speedup_models",
    srcs = [
        "base_speedup_model.cpp",
        "base_speedup_model.h",
        "linear_speedup_model.cpp",
        "log_speedup_model.cpp",
    ],
    hdrs = [
        "speedup_models.h",
    ],
    deps = [
        ":linreg",
    ],
)

cc_test(
    name = "test_speedup_models",
    size = "small",
    srcs = ["test_speedup_models.cpp"],
    linkopts = ["-lm"],
    deps = [
        ":speedup_models",
        "//third_party/catch:main",
        "//xiosim:catch_impl",
    ],
)

cc_library(
    name = "scheduler",
    srcs = ["scheduler.cpp"],
    hdrs = ["scheduler.h"],
    deps = [
        ":multiprocess",
        "//xiosim:libsim",
        "//xiosim:synchronization",
        "//xiosim:zesto-core",
        "//xiosim:zesto-structs",
    ],
)

cc_library(
    name = "buffer_manager",
    srcs = ["BufferManager.cpp"],
    hdrs = ["BufferManager.h"],
    deps = [
        ":multiprocess",
    ],
)

cc_library(
    name = "buffer_manager_consumer",
    srcs = ["BufferManagerConsumer.cpp"],
    hdrs = ["BufferManagerConsumer.h"],
    deps = [
        ":buffer",
        ":buffer_manager",
        ":handshake_container",
        "//xiosim:synchronization",
    ],
)

cc_library(
    name = "buffer_manager_producer",
    srcs = ["BufferManagerProducer.cpp"],
    hdrs = ["BufferManagerProducer.h"],
    deps = [
        ":buffer",
        ":buffer_manager",
        ":handshake_container",
    ],
)

cc_library(
    name = "linreg",
    srcs = ["linreg.cpp"],
    hdrs = ["linreg.h"],
)

cc_library(
    name = "buffer",
    hdrs = ["buffer.h"],
)

cc_library(
    name = "handshake_container",
    hdrs = ["handshake_container.h"],
    deps = [
        "//xiosim:x86",
    ],
)

cc_test(
    name = "test_handshake_serialization",
    size = "small",
    srcs = ["test_handshake_serialization.cpp"],
    deps = [
        ":handshake_container",
        "//third_party/catch:main",
        "//xiosim:catch_impl",
    ],
)
