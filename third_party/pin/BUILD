# Description:
#   Forwarding package, rules should depend on this, so it's easy to
#   overwrite if, say, we have the actual package at a different location.

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "pin",
    visibility = ["//visibility:public"],
    deps = ["@pin//:pin"],
)

cc_library(
    name = "xed",
    visibility = ["//visibility:public"],
    deps = ["@pin//:xed"],
)

filegroup(
    name = "cp_pinbin",
    srcs = ["@pin//:cp_pinbin"],
    visibility = ["//visibility:public"],
)
