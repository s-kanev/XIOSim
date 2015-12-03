# Description:
#   Cacti is a cache model distributed with McPAT.

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "lib",
    srcs = [
        "Ucache.cc",
        "arbiter.cc",
        "area.cc",
        "bank.cc",
        "basic_circuit.cc",
        "cacti_interface.cc",
        "component.cc",
        "crossbar.cc",
        "decoder.cc",
        "htree2.cc",
        "io.cc",
        "main.cc",
        "mat.cc",
        "nuca.cc",
        "parameter.cc",
        "powergating.cc",
        "router.cc",
        "subarray.cc",
        "technology.cc",
        "uca.cc",
        "wire.cc",
    ],
    hdrs = glob(["*.h"]),
    copts = [
        "-DPIN",
        "-DNTHREADS=1",
        "-Wno-all",  # mcpat is full of warnings, disable
    ],
    includes = ["."],
    linkopts = [
        "-lm",
        "-pthread",
    ],
    visibility = ["//visibility:public"],
)
