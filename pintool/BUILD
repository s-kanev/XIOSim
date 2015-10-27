package( default_visibility = ["//visibility:public"] )

cc_binary(
    name = "timing_sim",
    srcs = [
        "timing_sim.cpp",
        "timing_sim.h",
    ],
    deps = [
        "//:libsim",
        ":allocators",
        ":buffer_manager_consumer",
        ":ipc_queues",
        ":multiprocess",
        ":scheduler",
        ":ezOptionParser",
    ],
    linkopts = [ "-lrt", "-lconfuse" ]
)

cc_binary(
    name = "feeder_zesto.so",
    srcs = [
        "feeder.h",
        "feeder_zesto.cpp",
        "ildjit.h",
        "ildjit.cpp",
        "parse_speedup.h",
        "parse_speedup.cpp",
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
        "@pin//:pin",
        "@pin//:xed",
        ":legacy_instlib",
        ":buffer_manager_producer",
        ":ipc_queues",
        ":multiprocess",
        ":linreg",
        "//:zesto-bpred",
    ],
    linkopts = [ "-lrt", "-lm", "-ldl", "-pthread" ],
    #linkshared = 1 -- we really should, but the linker complains about non-PIC in libpin.a
)

cc_binary(
    name = "harness",
    srcs = [ "harness.cpp" ],
    deps = [
        ":multiprocess",
        ":ipc_queues",
        "@confuse//:main",
        ":ezOptionParser",
    ],
    linkopts = [ "-lrt", "-lm", "-lconfuse", "-pthread" ],
)

cc_library(
    name = "legacy_instlib",
    hdrs = glob([ "InstLib/*.H" ]),
    includes = [ "InstLib" ],
)

cc_library(
    name = "multiprocess",
    hdrs = [ "multiprocess_shared.h" ],
    srcs = [
        "boost_interprocess.h",
        "mpkeys.h",
        "mpkeys_impl.cpp",
        "multiprocess_shared.cpp",
        "shared_common.h",
        "shared_map.h",
        "shared_unordered_common.h",
        "shared_unordered_map.h",
    ],
    deps = [
        "//:misc",
        "//:synchronization",
        "@boost//:interprocess",
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
        "//:synchronization",
        ":multiprocess",
        ":speedup_models",
    ],
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

cc_library(
    name = "ipc_queues",
    hdrs = [ "ipc_queues.h" ],
    srcs = [ "ipc_queues.cpp" ],
    deps = [
        ":multiprocess",
    ],
)

cc_library(
    name = "scheduler",
    hdrs = [ "scheduler.h" ],
    srcs = [ "scheduler.cpp" ],
    deps = [
        ":multiprocess",
        "//:synchronization",
        "//:libsim",
        "//:zesto-core",
        "//:zesto-structs",
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
        "//:synchronization",
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
        "//:x86",
    ]
)

cc_library(
    name = "ezOptionParser",
    hdrs = [ "ezOptionParser_clean.hpp" ],
    srcs = [ "ezOptionParser.hpp" ],
)
