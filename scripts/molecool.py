#! /usr/bin/python
import os.path
import re

from xiosim_driver import *

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

def RunScalingTest():
#    for ncores in [1, 2, 4, 8, 16]:
    for ncores in [16]:
        xios = XIOSimDriver(PIN, PINTOOL)
        xios.AddCleanArch()
        env = ENVIRONMENT + "PARALLELIZER_THREADS=%d " % ncores
        xios.AddEnvironment(env)
        xios.AddPinOptions()
        xios.AddPintoolOptions()
        xios.AddMolecoolOptions()
        xios.AddZestoOptions(ZESTOCFG, MEMCFG)
        xios.AddZestoOut("%d.out" % ncores)
        xios.AddZestoCores(ncores)
        xios.AddILDJITOptions()
        xios.AddApp(PROGRAM, ARGS)
        xios.Exec()

if __name__ == "__main__":
    RunScalingTest()
