import shlex, subprocess

class XIOSimDriver(object):
    def __init__(self, PIN, PINTOOL):
        self.cmd = ""
        self.PIN = PIN
        self.PINTOOL = PINTOOL

    def AddCleanArch(self):
        self.cmd += "/usr/bin/setarch i686 -3BL "

    def AddEnvironment(self, env):
        self.cmd += "/usr/bin/env -i " + env + " "

    def AddPinOptions(self):
        self.cmd += self.PIN + " "
        self.cmd += "-xyzzy "
        self.cmd += "-separate_memory -pause_tool 1 -t "
        self.cmd += self.PINTOOL + " "

    def AddPintoolOptions(self):
        self.cmd += "-pipeline_instrumentation "

    def AddPinPointFile(self, file):
        self.cmd += "-ppfile %s " % file

    def AddMolecoolOptions(self):
        self.cmd += "-ildjit "

    def AddTraceFile(self, file):
        self.cmd += "-trace %s " % file

    def AddZestoOptions(self, cfg, mem_cfg):
        self.cmd += "-s "
        self.cmd += "-config " + cfg + " "
        self.cmd += "-config " + mem_cfg + " "

    def AddZestoOut(self, ofile):
        self.cmd += "-redir:sim " + ofile + " "

    def AddZestoHeartbeat(self, ncycles):
        self.cmd += "-heartbeat " + str(ncycles) + " "

    def AddZestoCores(self, ncores):
        self.cmd += "-cores " + str(ncores) + " "

    def AddZestoPowerFile(self, fname):
        self.cmd += "-power:rtp_file " + fname + " "

    def AddILDJITOptions(self):
        self.cmd += "-- iljit --static -O3 -M -N -R -T "

    def AddApp(self, program, args):
        self.cmd += "-- " + program + " " + args

    def Exec(self, stdin_file=None, stdout_file=None, stderr_file=None, cwd=None):
        print self.cmd

        #Provide input/output redirection
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
