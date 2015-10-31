licenses(["reciprocal"])

cc_library(
    name = "lib",
    visibility = ["//visibility:public"],
    srcs = [
        "area.cc",
        "bank.cc",
        "mat.cc",
        "main.cc",
        "Ucache.cc",
        "io.cc",
        "technology.cc",
        "basic_circuit.cc",
        "parameter.cc",
        "decoder.cc",
        "component.cc",
        "uca.cc",
        "subarray.cc",
        "wire.cc",
        "htree2.cc",
        "cacti_interface.cc",
        "router.cc",
        "nuca.cc",
        "crossbar.cc",
        "arbiter.cc",
        "powergating.cc",
    ],
    hdrs = glob([ "*.h" ]),
    includes = [ "." ],
    copts = [
        "-DPIN",
        "-DNTHREADS=1",
        "-Wno-all", # mcpat is full of warnings, disable
    ],
    linkopts = [ "-lm" , "-pthread" ],
)
