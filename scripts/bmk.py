#!/usr/bin/python
import os
import os.path
import shutil

class BenchmarkRun(object):

    def __init__(self, bmk, executable, args, input, output, error, name, expected):
        ''' Create a benchmark run instance. All paths are relative to the working
        directory.
        Arguments:
            bmk: Benchmark name, as defined in SPEC (e.g. 401.bzip2).
            executable: Binary name.
            args: Arguments for this benchmark run.
            input, output, error: Files to privide stdin, stdout, stderr.
            name: Name for the input (e.g. 'chicken' in 401.bzip2.chicken)
            expected: A list of files that the benchmark run is expecting in
                      the run directory. They'll be symlinked from the SPEC repo.
        '''
        self.bmk = bmk
        self.executable = executable
        self.args = args
        self.input = input
        self.output = output
        self.error = error
        self.directory = self.GetDir()
        self.name = "%s.%s" % (self.bmk, name)
        self.expected_files = expected


    def CreateRunDir(self, run_dir):
        ''' Create a directory -- the run will execute in that. If it exists,
        it will get deleted. Symlink all files that benchmark is expecting
        in the working directory, inlcuding the binary. '''
        if os.path.exists(run_dir):
            shutil.rmtree(run_dir)
        os.makedirs(run_dir)

        # symlink files that the benchmark is expecting
        # this includes the binary and the stdin file, if any
        for f in [self.executable, self.input] + self.expected_files:
            if not f:
                continue
            orig_path = os.path.join(self.directory, f)
            link_path = os.path.join(run_dir, f)
            os.symlink(orig_path, link_path)


    def GenerateBashArgs(self):
        args = self.args + " "
        if self.input:
            args += "< %s " % self.input
        if self.output:
            args += "> %s " % self.output
        if self.error:
            args += "2> %s " % self.error
        return args


    def GenerateConfigFile(self, run_dir):
        ''' Generate a config file for the benchmark run. All paths are relative
        to the working directory. '''
        lines = []
        lines.append("program {\n")
        lines.append("  run_path = \"%s\"\n" % run_dir)
        lines.append("  exe = \"./%s\"\n" % self.executable)
        args = "  args = \""
        args += self.GenerateBashArgs()
        args += "\"\n"
        lines.append(args)
        lines.append("}\n")
        return lines


    def NeedsPinPoints(self):
        return self.needs_pin_points
