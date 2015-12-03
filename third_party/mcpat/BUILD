# Description:
#   McPAT is a processor power model.

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "lib",
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
        "XML_Parse.h",
        "mcpat.h",
    ],
    copts = [
        "-Wno-all",  # mcpat is full of warnings, disable
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/mcpat/cacti:lib",
    ],
)
