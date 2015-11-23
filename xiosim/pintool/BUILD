package( default_visibility = ["//visibility:public"] )
licenses(["notice"]) #BSD

test_suite(
    name = "unit_tests",
    tags = [ "small" ],
)

cc_binary(
    name = "timing_sim",
    srcs = [
        "timing_sim.cpp",
        "timing_sim.h",
    ],
    deps = [
        "//xiosim:libsim",
        ":allocators",
        ":buffer_manager_consumer",
        ":multiprocess",
        ":scheduler",
        "//third_party/ezOptionParser:ezOptionParser",
        "//third_party/confuse:confuse",
    ],
    linkopts = [ "-lrt" ],
)

cc_binary(
    name = "feeder_zesto.so",
    srcs = [
        "feeder.h",
        "feeder_zesto.cpp",
        "ildjit.h",
        "ildjit.cpp",
        "sync_pthreads.h",
        "sync_pthreads.cpp",
        "syscall_handling.h",
        "syscall_handling.cpp",
        "ignore_ins.h",
        "ignore_ins.cpp",
        "replace_function.h",
        "replace_function.cpp",
        "roi.h",
        "roi.cpp",
        "speculation.h",
        "speculation.cpp",
        "utils.h",
        "utils.cpp",
    ],
    deps = [
        "//third_party/InstLib:legacy_instlib",
        ":buffer_manager_producer",
        ":multiprocess",
        ":linreg",
        ":parse_speedup",
        "//xiosim:zesto-bpred",
        "//third_party/pin:pin",
        "//third_party/pin:xed",
    ],
    linkopts = [ "-lrt", "-lm", "-ldl", "-pthread" ],
    linkshared = 1
)

cc_binary(
    name = "harness",
    srcs = [ "harness.cpp" ],
    deps = [
        ":multiprocess",
        "//third_party/ezOptionParser:ezOptionParser",
        "//third_party/confuse:confuse",
    ],
    linkopts = [ "-lrt", "-lm", "-pthread" ],
)

cc_library(
    name = "multiprocess",
    hdrs = [ "multiprocess_shared.h" ],
    srcs = [
        "boost_interprocess.h",
        "ipc_queues.h",
        "ipc_queues.cpp",
        "mpkeys.h",
        "mpkeys_impl.cpp",
        "multiprocess_shared.cpp",
        "shared_common.h",
        "shared_map.h",
        "shared_unordered_common.h",
        "shared_unordered_map.h",
    ],
    deps = [
        "//xiosim:misc",
        "//xiosim:synchronization",
        "//third_party/boost:interprocess",
    ],
)

cc_library(
    name = "allocators",
    hdrs = [
        "allocators_impl.h",
    ],
    srcs = [
        "base_allocator.h",
        "base_allocator.cpp",
        "gang_allocator.cpp",
        "local_opt_allocator.cpp",
        "penalty_allocator.cpp",
        "optimization_targets.cpp",
    ],
    deps = [
        "//xiosim:synchronization",
        ":multiprocess",
        ":speedup_models",
    ],
)

cc_test(
    name = "test_allocation",
    srcs = [ "test_allocation.cpp" ],
    deps = [
        ":allocators",
        ":parse_speedup",
        "//third_party/catch:main",
    ],
    data = [ "test_data/loop_speedup_data.csv" ],
    linkopts = [ "-lrt", "-lm", "-ldl", "-pthread" ],
    size = "small",
)

cc_library(
    name = "parse_speedup",
    srcs = [ "parse_speedup.cpp" ],
    hdrs = [ "parse_speedup.h" ],
    deps = [ "//third_party/boost:tokenizer" ],
)

cc_test(
    name = "test_parse_speedup",
    srcs = [ "test_parse_speedup.cpp" ],
    deps = [
        "//xiosim:catch_impl",
        ":parse_speedup",
        "//third_party/catch:main",
    ],
    data = [ "test_data/loop_speedup_data.csv" ],
    size = "small",
)


cc_library(
    name = "speedup_models",
    hdrs = [
        "speedup_models.h",
    ],
    srcs = [
        "base_speedup_model.h",
        "base_speedup_model.cpp",
        "log_speedup_model.cpp",
        "linear_speedup_model.cpp",
    ],
    deps = [
        ":linreg",
    ],
)

cc_test(
    name = "test_speedup_models",
    srcs = [ "test_speedup_models.cpp" ],
    deps = [
        "//xiosim:catch_impl",
        ":speedup_models",
        "//third_party/catch:main",
    ],
    linkopts = [ "-lm" ],
    size = "small",
)

cc_library(
    name = "scheduler",
    hdrs = [ "scheduler.h" ],
    srcs = [ "scheduler.cpp" ],
    deps = [
        ":multiprocess",
        "//xiosim:synchronization",
        "//xiosim:libsim",
        "//xiosim:zesto-core",
        "//xiosim:zesto-structs",
    ],
)

cc_library(
    name = "buffer_manager",
    hdrs = [ "BufferManager.h" ],
    srcs = [ "BufferManager.cpp" ],
    deps = [
        ":multiprocess",
    ],
)

cc_library(
    name = "buffer_manager_consumer",
    hdrs = [ "BufferManagerConsumer.h" ],
    srcs = [ "BufferManagerConsumer.cpp" ],
    deps = [
        ":buffer",
        ":buffer_manager",
        ":handshake_container",
        "//xiosim:synchronization",
    ],
)

cc_library(
    name = "buffer_manager_producer",
    hdrs = [ "BufferManagerProducer.h" ],
    srcs = [ "BufferManagerProducer.cpp" ],
    deps = [
        ":buffer",
        ":buffer_manager",
        ":handshake_container",
    ],
)

cc_library(
    name = "linreg",
    hdrs = [ "linreg.h" ],
    srcs = [ "linreg.cpp" ],
)

cc_library(
    name = "buffer",
    hdrs = [ "buffer.h" ],
)

cc_library(
    name = "handshake_container",
    hdrs = [ "handshake_container.h" ],
    deps = [
        "//xiosim:x86",
    ]
)

cc_test(
    name = "test_handshake_serialization",
    srcs = [ "test_handshake_serialization.cpp" ],
    deps = [
        "//xiosim:catch_impl",
        ":handshake_container",
        "//third_party/catch:main",
    ],
    size = "small",
)
