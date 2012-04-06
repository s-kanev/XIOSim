#! /usr/bin/python
import shlex, subprocess
import os.path
import re

ILDJIT_HOME="/group/brooks/xan/Molecool/ildjit"
PIN_HOME="/home/skanev/pin/pin-2.10-45467-gcc.3.4.6-ia32_intel64-linux"

#envoronment variables are explicitly added to a clean environment
ENVIRONMENT="ILDJIT_HOME=%s ILDJIT_PATH=.:%s/lib/cscc/lib:%s/lib/gcc4cli LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:/lib:%s/lib/iljit:%s/ia32/runtime PKG_CONFIG_PATH=%s/lib/pkgconfig PATH=%s/bin:/lib:/usr/lib:/usr/bin ILDJIT_CACHE=/home/skanev HOME=/home/skanev " % (ILDJIT_HOME, ILDJIT_HOME, ILDJIT_HOME, ILDJIT_HOME, PIN_HOME, ILDJIT_HOME, ILDJIT_HOME) 
# PARALLELIZER_AUTOMATIC_PARALLELIZATION_SINGLE_THREAD=1"
#PARALLELIZER_AUTOMATIC_PARALLELIZATION=0"
#PARALLELIZER_AUTOMATIC_PARALLELIZATION=1"
#LD_ASSUME_KERNEL=2.4.1"

PROJROOT="/group/brooks/xan/Molecool"

PROGRAM="/home/skanev/iljit_regression/test0/test.cil"
ARGS="200000"
#ARGS="2"

PIN=PIN_HOME + "/ia32/bin/pinbin"
PINTOOL="%s/bin/feeder_zesto.so" % PROJROOT
ZESTOCFG="%s/feeder_zesto/config/A2.cfg" % PROJROOT
MEMCFG="%s/feeder_zesto/dram-config/DDR2-800-5-5-5.cfg" % PROJROOT

#rm ./test.cil_*

class XIOSimDriver(object):
    def __init__(self):
        self.cmd = ""

    def AddCleanArch(self):
        self.cmd += "/usr/bin/setarch i686 -3BL "

    def AddEnvironment(self, env):
        self.cmd += "/usr/bin/env -i " + env + " "

    def AddPinOptions(self):
        self.cmd += PIN + " "
#        self.cmd += "-xyzzy -profile 1 -statistic 1 -profile_period 10000 "
#        self.cmd += "-mesgon stats -mesgon phase "
#        self.cmd += "-xyzzy -ctxt_fp 0 "
        self.cmd += "-xyzzy -allow_AVX_support 0 "
        self.cmd += "-separate_memory -pause_tool 1 -t "
        self.cmd += PINTOOL + " "

    def AddMolecoolOptions(self):
        self.cmd += "-ildjit -pipeline_instrumentation "

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

    def AddILDJITOptions(self):
        self.cmd += "-- iljit --static -O3 -M -N -R -T "

    def AddApp(self, program, args):
        self.cmd += program + " " + args

    def Exec(self):
        # Clean ILDJIT temp files
        app = os.path.basename(PROGRAM)
        cwd_files = os.listdir(".")
        for file in cwd_files:
            if re.match(app + "_.*", file):
                delcmd = "rm " + file
                child = subprocess.Popen(shlex.split(delcmd))
                child.wait()

        print self.cmd
        child = subprocess.Popen(shlex.split(self.cmd), close_fds=True)
        retcode = child.wait()

        if retcode == 0:
            print "Completed"
        else:
            print "Failed! Error code: %d" % retcode

def RunScalingTest():
#    for ncores in [1, 2, 4, 8, 16]:
    for ncores in [16]:
        xios = XIOSimDriver()
        xios.AddCleanArch()
        env = ENVIRONMENT + "PARALLELIZER_THREADS=%d " % ncores
        xios.AddEnvironment(env)
        xios.AddPinOptions()
        xios.AddMolecoolOptions()
        xios.AddZestoOptions(ZESTOCFG, MEMCFG)
        xios.AddZestoOut("%d.out" % ncores)
        xios.AddZestoCores(ncores)
        xios.AddILDJITOptions()
        xios.AddApp(PROGRAM, ARGS)
        xios.Exec()

if __name__ == "__main__":
    RunScalingTest()
