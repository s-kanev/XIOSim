# Description:
#   XIOSim, a detailed x86 microarchitectural simulator.
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # BSD

exports_files(["LICENSE"])

filegroup(
    name = "xiosim",
    srcs = [
        "//third_party/pin:cp_pinbin",
        "//xiosim/pintool:feeder_zesto.so",
        "//xiosim/pintool:harness",
        "//xiosim/pintool:timing_sim",
    ],
)

test_suite(
    name = "all_unit",
    tags = ["small"],
    tests = [
        "//xiosim:unit_tests",
        "//xiosim/pintool:unit_tests",
    ],
)
