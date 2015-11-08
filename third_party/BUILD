# This wrapper package should disappear once the config.h problem
# in confuse.BUILD gets resolved.
cc_library(
    visibility = ["//visibility:public"],
    name = "confuse",
    srcs = [
        "@confuse//:srcs",
        "confuse/config.h",
        "confuse/lexer.c",
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
