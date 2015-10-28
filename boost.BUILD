licenses(["notice"]) # boost licence

cc_library(
    visibility = ["//visibility:public"],
    name = "accumulators",
    hdrs = glob([
        "boost_1_54_0/boost/accumulators/accumulators.hpp",
        "boost_1_54_0/boost/accumulators/statistics/*.hpp",
    ]),
    includes = ["boost_1_54_0"],
)

cc_library(
    visibility = ["//visibility:public"],
    name = "interprocess",
    hdrs = glob([
        "boost_1_54_0/boost/interprocess/*.hpp",
        "boost_1_54_0/boost/interprocess/allocators/*.hpp",
        "boost_1_54_0/boost/interprocess/containers/*.hpp",
        "boost_1_54_0/boost/interprocess/sync/*.hpp",
    ]),
    includes = ["boost_1_54_0"],
)

cc_library(
    visibility = ["//visibility:public"],
    name = "tokenizer",
    hdrs = glob([
        "boost_1_54_0/boost/tokenizer.hpp",
        "boost_1_54_0/boost/algorithm/string/predicate.hpp",
    ]),
    includes = ["boost_1_54_0"],
)
