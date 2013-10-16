#! /usr/bin/python
import os.path

import xiosim_driver as xd
import spec

PIN_HOME="/home/skanev/pin/2.12"
PIN= PIN_HOME + "/pin.sh"

PROJROOT = "/home/skanev/xiosim"
RESULT_DIR = "/home/skanev/spec_out"


PINTOOL = "/home/skanev/jnk/feeder_zesto.so"
ZESTOCFG = "%s/config/N.cfg" % PROJROOT # Nehalem configuration file
MEMCFG = "%s/dram-config/DDR3-1600-9-9-9.cfg" % PROJROOT

def RunSPECBenchmark(name):
    run = spec.GetRun(name)
    if run == None:
        raise Exception("Wrong benchmark!")

    xios = xd.XIOSimDriver(PIN, PINTOOL)
    xios.AddCleanArch()
    xios.AddEnvironment("LD_LIBRARY_PATH=/home/skanev/lib")
    xios.AddPinOptions()
    xios.AddPintoolOptions()
    xios.AddPinPointFile("%s/%s.pintool.1.pp" % (run.directory, name))
    xios.AddTraceFile("%s/%s.trace" % (RESULT_DIR, name))
    xios.AddZestoOptions(ZESTOCFG, MEMCFG)
    xios.AddZestoOut("%s/%s.simout" % (RESULT_DIR, name))
    xios.AddApp(run.executable, run.args)
    print xios.cmd

    # Adjust bmk input/output files to appropriate dirs
    inp = out = err = None
    if run.input:
        inp = "%s/%s" % (run.directory, run.input)
    if run.output:
        out = "%s/%s" % (RESULT_DIR, run.output)
    if run.error:
        err = "%s/%s" % (RESULT_DIR, run.error)

    xios.Exec(inp, out, err, run.directory)

if __name__ == "__main__":
    RunSPECBenchmark("401.bzip2.chicken")
