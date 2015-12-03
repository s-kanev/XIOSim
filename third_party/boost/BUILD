# Description:
#   Forwarding package, rules should depend on this, so it's easy to
#   overwrite if, say, we have the actual package at a different location.

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "accumulators",
    visibility = ["//visibility:public"],
    deps = ["@boost//:accumulators"],
)

cc_library(
    name = "interprocess",
    visibility = ["//visibility:public"],
    deps = ["@boost//:interprocess"],
)

cc_library(
    name = "tokenizer",
    visibility = ["//visibility:public"],
    deps = ["@boost//:tokenizer"],
)
