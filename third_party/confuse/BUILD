# This wrapper package should disappear once the config.h problem
# in confuse.BUILD gets resolved.
package( default_visibility = ["//visibility:public"] )
licenses(["notice"])

cc_library(
    name = "confuse",
    srcs = [
        "@confuse//:srcs",
        "config.h",
        "lexer.c",
    ],
    copts = [
        "-DHAVE_CONFIG_H",
        "-Ithird_party/confuse",
        "-Wno-unused-but-set-parameter",
        "-Wno-error", # -Wno-discarded-qualifiers not supported any more
    ],
    deps = [
        "@confuse//:main",
    ],
)
