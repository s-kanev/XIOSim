licenses(["notice"])

# Forwarding package, rules should depend on this, so it's easy to
# overwrite if, say, we have the actual package at a different location.

cc_library(
    visibility = ["//visibility:public"],
    name = "pin",
    deps = ["@pin//:pin"],
)

cc_library(
    visibility = ["//visibility:public"],
    name = "xed",
    deps = ["@pin//:xed"],
)

filegroup(
    visibility = ["//visibility:public"],
    name = "cp_pinbin",
    srcs = ["@pin//:cp_pinbin"],
)
