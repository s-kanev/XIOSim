#!/usr/bin/env python
import os.path

import xiosim_driver as xd
import spec

# Configuration params
RUN_DIR_ROOT = "/home/skanev/spec_out" # Benchmarks will execute in subdirectories of this
RESULT_DIR = "/home/skanev/spec_out"   # Results will be written here
CONFIG_FILE = "config/N.cfg"           # Starting config file (relative to XIOSIM_TREE)

def CreateDriver():
    PIN_ROOT = os.environ["PIN_ROOT"]
    XIOSIM_INSTALL = os.environ["XIOSIM_INSTALL"]
    XIOSIM_TREE = os.environ["XIOSIM_TREE"]
    env = None
    use_own_lib = ("XIOSIM_ANCIENT_LIB" in os.environ)
    if use_own_lib:
        env = "LD_LIBRARY_PATH=/home/skanev/lib"
    xio = xd.XIOSimDriver(PIN_ROOT, XIOSIM_INSTALL, XIOSIM_TREE,
                          clean_arch=True, env=env)
    return xio


def WriteBmkConfig(xio, run, run_dir):
    ''' Create a temp benchmark config file in the run directory.'''
    cfg_file = run.GenerateConfigFile(run_dir)
    bmk_cfg = os.path.join(run_dir, "bmk_cfg")
    with open(bmk_cfg, "w") as f:
        for l in cfg_file:
            f.write(l)
    return bmk_cfg


def WriteArchConfig(xio, run, run_dir, start_config, changes):
    ''' Create a temp config file in the run directory,
    starting with @start_config and applying a dictionary or
    paramter changes. '''
    cfg_file = os.path.join(run_dir, os.path.basename(start_config))
    cfg_contents = xio.GenerateConfigFile(start_config, changes)
    with open(cfg_file, "w") as f:
        for l in cfg_contents:
            f.write(l)
    return cfg_file


def RunSPECBenchmark(name):
    run = spec.GetRun(name)
    if run == None:
        raise Exception("Wrong benchmark!")

    # Create a brand new directory to execute in
    run_dir = os.path.join(RUN_DIR_ROOT, name)
    run.CreateRunDir(run_dir)
    xio = CreateDriver()

    # Grab a benchmark description file
    bmk_cfg = WriteBmkConfig(xio, run, run_dir)
    xio.AddBmks(bmk_cfg)

    # We want to get results in a separate results directory, a tad cleaner
    outfile = os.path.join(RESULT_DIR, "%s.sim.out" % name)
    repl = {
        "system_cfg.output_redir" : outfile,
    }
    orig_cfg = os.path.join(xio.GetTreeDir(), CONFIG_FILE)
    arch_cfg = WriteArchConfig(xio, run, run_dir, orig_cfg, repl)
    xio.AddConfigFile(arch_cfg)

    xio.AddPinOptions()
    # Grab a pinpoints file from the original SPEC repo
    ppfile = os.path.join(run.directory, "%s.pintool.1.pp" % name)
    xio.AddPinPointFile(ppfile)


    out_file = os.path.join(run_dir, "harness.out")
    err_file = os.path.join(run_dir, "harness.err")
    ret = xio.Exec(stdout_file=out_file, stderr_file=err_file, cwd=run_dir)
    if ret != 0:
        raise Exception("XIOSim run failed (errcode %d)" % ret)

if __name__ == "__main__":
    RunSPECBenchmark("401.bzip2.chicken")
