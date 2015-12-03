# Description:
#   Forwarding package, rules should depend on this, so it's easy to
#   overwrite if, say, we have the actual package at a different location.

licenses(["notice"])

exports_files(["LICENSE"])

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "confuse",
    deps = [":confuse_configed"],
)

cc_library(
    name = "confuse_configed",
    srcs = [
        "@confuse//:srcs",
        "config.h",
        "lexer.c",
    ],
    copts = [
        "-DHAVE_CONFIG_H",
        "-Ithird_party/confuse",
        "-Wno-unused-but-set-parameter",
        "-Wno-error",  # -Wno-discarded-qualifiers not supported any more
    ],
    visibility = ["//visibility:private"],
    deps = [
        "@confuse//:main",
    ],
)
