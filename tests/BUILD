# Description:
#   XIOSim ubenchmarks for integration tests

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # BSD

exports_files(["LICENSE"])

filegroup(
    name = "ubench",
    srcs = select({
        ":k8": glob(["k8/*"]),
        ":piii": glob(["piii/*"]),
    }),
)

config_setting(
    name = "k8",
    values = {"cpu": "k8"},
)

config_setting(
    name = "piii",
    values = {"cpu": "piii"},
)
