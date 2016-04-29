# Invoke objcopy to make a list of symbols in an archive weak

ar_file_type = FileType([".a"])

def _impl(ctx):
    output = ctx.outputs.out
    input = ctx.file.srcs
    syms = ctx.attr.symbols

    sym_params = []
    for sym in syms:
        sym_params += ["-W", sym]

    ctx.action(
        inputs = [input],
        outputs = [output],
        progress_message = "Running objcopy on %s" % input.short_path,
        executable = ctx.fragments.cpp.objcopy_executable,
        arguments = sym_params + [input.path, output.path],
    )

cc_weaken = rule(
    implementation = _impl,
    fragments = ["cpp"],
    attrs = {
        "srcs": attr.label(mandatory=True, allow_files=ar_file_type, single_file=True),
        "symbols" : attr.string_list(),
    },
    outputs = {"out": "%{name}.weak.a"},
)

