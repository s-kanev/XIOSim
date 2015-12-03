# Description:
#   Forwarding package, rules should depend on this, so it's easy to
#   overwrite if, say, we have the actual package at a different location.

licenses(["notice"])

exports_files(["LICENSE"])

cc_library(
    name = "main",
    visibility = ["//visibility:public"],
    deps = ["@catch//:main"],
)
