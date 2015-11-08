import os
import shlex
import subprocess

class XIOSimDriver(object):
    def __init__(self, PIN_ROOT, INSTALL_DIR, TREE_DIR, TARGET_ARCH, clean_arch=None, env=None):
        ''' TARGET_ARCH uses bazel notation. "piii" is ia32, "k8" is ia64.
        '''
        self.cmd = ""
        #self.PIN = os.path.join(TREE_DIR, "bazel-xiosim/external/pin/pin-2.14-67254-gcc.4.4.7-linux/pin.sh")
        # Ugh, we still need PIN_ROOT. Apparently the "bazel-xiosim" symlink above is
        # actually "bazel-{cwd}", so it will change if we checkout under a different
        # directory. TODO: Figure out a way around this. 
        self.PIN = os.path.join(PIN_ROOT, "pin.sh")
        self.INSTALL_DIR = INSTALL_DIR
        self.TREE_DIR = TREE_DIR
        self.TARGET_ARCH = TARGET_ARCH
        if clean_arch:
            self.AddCleanArch()
        if env:
            self.AddEnvironment(env)
        self.AddHarness()

    def AddCleanArch(self):
        self.cmd += "/usr/bin/setarch "
        if self.TARGET_ARCH == "piii":
            self.cmd += "i686 -BR "
        else:
            self.cmd += "x86_64 -R "

    def AddEnvironment(self, env):
        self.cmd += "/usr/bin/env -i " + env + " "

    def AddHarness(self):
        self.cmd +=  os.path.join(self.INSTALL_DIR, "harness") + " "

    def AddBmks(self, bmk_cfg):
        self.cmd += "-benchmark_cfg " + bmk_cfg + " "

    def AddPinOptions(self):
        self.cmd += "-pin " + self.PIN + " "
        self.cmd += "-xyzzy "
        self.cmd += "-pause_tool 1 "
        self.cmd += "-catch_signals 0 "
        self.cmd += "-t " + os.path.join(self.INSTALL_DIR, "feeder_zesto.so") + " "

    def AddPintoolOptions(self, num_cores):
        self.cmd += "-num_cores %d " % num_cores

    def AddPinPointFile(self, file):
        self.cmd += "-ppfile %s " % file

    def AddInstLength(self, ninsn):
        self.cmd += "-length %d " % ninsn

    def AddSkipInst(self, ninsn):
        self.cmd += "-skip %d " % ninsn

    def AddMolecoolOptions(self):
        self.cmd += "-ildjit "

    def AddIgnoreOptions(self):
        self.cmd += "-ignore_api "

    def AddROIOptions(self):
        self.cmd += "-roi "

    def AddIgnorePCOptions(self, pcs):
        self.AddIgnoreOptions()
        self.cmd += "-ignore_pcs %s " % pcs

    def AddReplaceOptions(self, func):
        self.AddIgnoreOptions()
        self.cmd += "-ignore_functions %s " % func

    def AddTraceFile(self, file):
        self.cmd += "-trace %s " % file

    def AddZestoOptions(self, cfg):
        self.cmd += "-s "
        self.cmd += "-config " + cfg + " "

    def Exec(self, stdin_file=None, stdout_file=None, stderr_file=None, cwd=None):
        print self.cmd

        if cwd:
            self.run_dir = cwd

        # Provide input/output redirection
        if stdin_file:
            stdin = open(stdin_file, "r")
        else:
            stdin = None

        if stdout_file:
            stdout = open(stdout_file, "w")
        else:
            stdout = None

        if stderr_file:
            stderr=open(stderr_file, "w")
        else:
            stderr = None

        #... and finally launch command
        child = subprocess.Popen(shlex.split(self.cmd), close_fds=True, stdin=stdin, stdout=stdout, stderr=stderr, cwd=cwd)
        retcode = child.wait()

        if retcode == 0:
            print "Completed"
        else:
            print "Failed! Error code: %d" % retcode
        return retcode

    def GetRunDir(self):
        return self.run_dir

    def GetSimOut(self):
        return os.path.join(self.GetRunDir(), "sim.out")

    def GetTreeDir(self):
        return self.TREE_DIR

    def GenerateTestBmkConfig(self, test, num_copies=1):
        res = []
        res.append("program {\n")
        res.append("  exe = \"%s\"\n" % os.path.join(self.TREE_DIR, "tests", self.TARGET_ARCH, test))
        append_pid = ""
        if num_copies > 1:
            append_pid = ".$$"
        res.append("  args = \"> %s%s.out 2> %s%s.err\"\n" % (test, append_pid,
                                                              test, append_pid))
        res.append("  instances = %d\n" % num_copies)
        res.append("}\n")
        return res

    def GenerateConfigFile(self, config, changes):
        ''' Generate a config file from an existing one, by replacing some stats.
            config: path to the original config file.
            changes: A dictionary with values in config to replace.
                Ex. "core_cfg.fetch_cfg.instruction_queue_size" -> "18"
        '''
        with open(config) as lines:
            curr_path = []
            result = []
            for line in lines:
                if "=" in line:
                    name = line.split("=")[0].strip()
                    val = line.split("=")[1].strip()

                    # turn path in a "."-separated list
                    item = ".".join(curr_path+[name])
                    # and check if are trying to replace it
                    if changes and item in changes:
                        line = line.replace(val, changes[item])
                elif "{" in line:
                    pre_brace = line.split("{")[0].strip()
                    # ignore optional name, unless it actually matters
                    cat = pre_brace.split(" ")[0]
                    if cat == "exeu":
                        curr_path.append(pre_brace)
                    else:
                        curr_path.append(cat)
                elif "}" in line:
                    curr_path.pop()
                result.append(line)
            return result
