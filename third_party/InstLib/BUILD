# Description:
#   InstLib is a library with pin utility functions.
#   This is an old version that was distributed with Pin 2.12, because
#   we rely on the various controller classes (that Intel removed in favor
#   of PinPlay).

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Intel Open Source license

exports_files(["LICENSE"])

cc_library(
    name = "legacy_instlib",
    hdrs = glob([
        "legacy_instlib.H",
        # Below should be in srcs, but bazel doesn't like the non-standard header
        # extension there.
        "*.H"
    ]),
)
