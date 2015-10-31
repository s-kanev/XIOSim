licenses(["reciprocal"])

cc_library(
    name = "lib",
    visibility = ["//visibility:public"],
    srcs = glob([
        "XML_Parse.cc",
        "array.cc",
        "basic_components.cc",
        "core.cc",
        "interconnect.cc",
        "iocontrollers.cc",
        "logic.cc",
        "slave.cc",
        "memoryctrl.cc",
        "noc.cc",
        "processor.cc",
        "sharedcache.cc",
        "xmlParser.cc",
        "*.h",
    ]),
    hdrs = [
        "mcpat.h",
        "XML_Parse.h",
    ],
    deps = [
        "//third_party/mcpat/cacti:lib",
    ],
    copts = [
        "-Wno-reorder",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
    ],
)
