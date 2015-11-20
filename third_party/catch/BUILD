licenses(["notice"])

# Forwarding package, rules should depend on this, so it's easy to
# overwrite if, say, we have the actual package at a different location.

cc_library(
    visibility = ["//visibility:public"],
    name = "main",
    deps = ["@catch//:main"],
)
