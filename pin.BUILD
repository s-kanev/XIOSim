licenses(["notice"]) # Intel open source

PIN_PATH = "pin-2.14-67254-gcc.4.4.7-linux"
XED_PATH = PIN_PATH + "/extras/xed2-%s/"

cc_library(
    visibility = ["//visibility:public"],
    name = "xed",
    hdrs = select({
        ":k8" : [ XED_PATH % "intel64" + "include/xed-interface.h"],
        ":piii" : [ XED_PATH % "ia32" + "include/xed-interface.h"],
    }),
    includes = select({
        ":k8" : [ XED_PATH % "intel64" + "include"],
        ":piii" : [ XED_PATH % "ia32" + "include"],
    }),
    srcs = select({
        ":k8" : [ XED_PATH % "intel64" + "lib/libxed.a"],
        ":piii" : [ XED_PATH % "ia32" + "lib/libxed.a"],
    }),
)

cc_library(
    visibility = ["//visibility:public"],
    name = "pin",
    includes = [
        PIN_PATH + "/source/include/pin",
        PIN_PATH + "/source/include/pin/gen",
        PIN_PATH + "/extras/components/include",
        PIN_PATH + "/source/tools/InstLib",
    ],
    defines = select({
        ":k8" : [ "TARGET_IA32E", "HOST_IA32E", "TARGET_LINUX" ],
        ":piii" : [ "TARGET_IA32", "HOST_IA32", "TARGET_LINUX" ],
    }),
    srcs = select({
        ":k8" : [
            PIN_PATH + "/intel64/lib/libpin.a",
            PIN_PATH + "/intel64/lib-ext/libelf.so",
            PIN_PATH + "/intel64/lib-ext/libdwarf.so",
        ],
        ":piii" : [
            PIN_PATH + "/ia32/lib/libpin.a",
            PIN_PATH + "/ia32/lib-ext/libelf.so",
            PIN_PATH + "/ia32/lib-ext/libdwarf.so",
        ],
    }),
)

config_setting(
    name = "k8",
    values = { "cpu" : "k8" }
)

config_setting(
    name = "piii",
    values = { "cpu" : "piii" }
)
