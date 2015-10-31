def gen_list(component, dirs, extra_deps=[]):
    for dir_ in dirs:
        # Generate a filegroup for each cpp in the directory
        native.filegroup(
            name = "%s" % dir_,
            srcs = native.glob(
                [ "%s/*.cpp" % dir_],
                exclude = [
                    "%s/repeater-default.cpp" % dir_, # for now
                    "%s/MC-dramsim.cpp" % dir_,       # for now
                ]
            )
        )

        # Generate {DIR}.list.h, which includes each cpp in the directory
        native.genrule(
            name = "gen-%s-list" % dir_,
            srcs = [ ":%s" % dir_ ],
            cmd = 'echo -n $(locations :%s) | xargs -d" " -I{} echo \'#include "{}"\' > $@' % dir_,
            outs = [ "%s.list.h" % dir_ ],
        )

    hdrs = [ "zesto-%s.h" % component ]
    # Add cpp filegroups, se we track them as dependants
    # We don't want to add them as srcs, or they'll get compiled individually
    for dir_ in dirs:
        hdrs += [ ":%s" % dir_ ]

    srcs = [ "zesto-%s.cpp" % component ]
    for dir_ in dirs:
        srcs += [ "%s.list.h" % dir_ ]

    native.cc_library(
        name = "zesto-%s" % component,
        hdrs = hdrs,
        srcs = srcs,
        deps = [
            ":stats",
            ":x86",
        ] + extra_deps,
    )
