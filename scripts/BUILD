# Description:
#   XIOSim integration tests

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # BSD

exports_files(["LICENSE"])

test_suite(
    name = "integration_tests",
    tags = ["large"],
)

py_test(
    name = "Fib1Test",
    size = "large",
    srcs = [":integration_framework"],
    args = ["Fib1Test"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "NoneTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["NoneTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "Fib1LengthTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["Fib1LengthTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "Fib1SkipTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["Fib1SkipTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "Fib1PinPointTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["Fib1PinPointTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "Fib1PinPointsTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["Fib1PinPointsTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "ROITest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["ROITest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "ReplaceTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["ReplaceTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "PowerTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["PowerTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "DFSTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["DFSTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "Fib2Test",
    size = "large",
    srcs = [":integration_framework"],
    args = ["Fib2Test"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "REPTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["REPTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "SegfTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["SegfTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "ChaseTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["ChaseTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "PrefetchTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["PrefetchTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "IgnorePCTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["IgnorePCTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "TimeTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["TimeTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

py_test(
    name = "AttachTest",
    size = "large",
    srcs = [":integration_framework"],
    args = ["AttachTest"],
    data = [":integration_data"],
    main = "run_tests.py",
)

# Framework files to launch the simulator and define tests
filegroup(
    name = "integration_framework",
    srcs = [
        "run_tests.py",
        "xiosim_driver.py",
        "xiosim_stat.py",
    ],
    visibility = ["//visibility:private"],
)

# The actual simulator and simulated programs
filegroup(
    name = "integration_data",
    srcs = [
        "//:xiosim",
        "//tests:ubench",
    ],
    visibility = ["//visibility:private"],
)
