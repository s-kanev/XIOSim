#!/usr/bin/env python

import copy
import os
import shutil
import sys
import tempfile
import unittest

import xiosim_driver as xd
import xiosim_stat as xs


def CreateDriver():
    PIN_ROOT = os.environ["PIN_ROOT"]
    XIOSIM_INSTALL = os.environ["XIOSIM_INSTALL"]
    XIOSIM_TREE = os.environ["XIOSIM_TREE"]
    xio = xd.XIOSimDriver(PIN_ROOT, XIOSIM_INSTALL, XIOSIM_TREE)
    return xio


class XIOSimTest(unittest.TestCase):
    ''' Test fixtures for XIOSim end-to-end tests.'''
    def setUp(self):
        ''' Set up a driver with common options and a temp run directory.'''
        self.xio = CreateDriver()
        self.run_dir = tempfile.mkdtemp()
        self.clean_run_dir = ("LEAVE_TEST_DIR" not in os.environ)
        self.setDriverParams()
        self.expected_vals = []


    def tearDown(self):
        if self.clean_run_dir:
            shutil.rmtree(self.xio.GetRunDir())


    def runAndValidate(self):
        ''' Execute XIOSimDriver and check output for golden values of stats.'''
        out_file = os.path.join(self.run_dir, "harness.out")
        err_file = os.path.join(self.run_dir, "harness.err")
        ret = self.xio.Exec(stdout_file=out_file, stderr_file=err_file, cwd=self.run_dir)
        self.assertEqual(ret, 0, "XIOSim run failed (errcode %d)" % ret)

        for re, golden_val in self.expected_vals:
            val = xs.GetStat(self.xio.GetSimOut(), re)
            res = xs.ValidateStat(val, golden_val)
            self.assertEqual(res, True, "%s: expected %.2f, got %.2f" %
                                        (re, golden_val, val))

    def writeTestBmkConfig(self, bmk, num_copies=1):
        ''' Create a temp benchmark config file in the test run directory.'''
        cfg_file = self.xio.GenerateTestBmkConfig(bmk, num_copies)
        bmk_cfg = os.path.join(self.run_dir, "%s.cfg" % bmk)
        with open(bmk_cfg, "w") as f:
            for l in cfg_file:
                f.write(l)
        return bmk_cfg

    def writeTestConfig(self, start_config, changes):
        ''' Create a temp config file in the test run directory,
        starting with @start_config and applying a dictionary or
        paramter changes. '''
        cfg_file = os.path.join(self.run_dir, os.path.basename(start_config))
        cfg_contents = self.xio.GenerateConfigFile(start_config, changes)
        with open(cfg_file, "w") as f:
            for l in cfg_contents:
                f.write(l)
        return cfg_file


class Fib1Test(XIOSimTest):
    ''' End-to-end test with a single binary running from start to end.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "N.cfg"))

    def setUp(self):
        super(Fib1Test, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 118369.0))

    def runTest(self):
        self.runAndValidate()


class NoneTest(XIOSimTest):
    ''' End-to-end test with the 'none' core model.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "none.cfg"))

    def setUp(self):
        super(NoneTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 118369.0))

    def runTest(self):
        self.runAndValidate()


class Fib1LengthTest(XIOSimTest):
    ''' End-to-end test with a single binary running for X instructions.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddInstLength(10000)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "N.cfg"))

    def setUp(self):
        super(Fib1LengthTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 9900.0))

    def runTest(self):
        self.runAndValidate()


class Fib1SkipTest(XIOSimTest):
    ''' End-to-end test with a single binary skipping first X instructions.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddSkipInst(50000)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "N.cfg"))

    def setUp(self):
        super(Fib1SkipTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 68369.0))

    def runTest(self):
        self.runAndValidate()


class Fib1PinPointTest(XIOSimTest):
    ''' End-to-end test with a single binary and one pinpoint.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        ppfile = os.path.join(self.xio.GetTreeDir(), "tests",
                              "fib..pintool.1.pp")
        self.xio.AddPinPointFile(ppfile)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "N.cfg"))

    def setUp(self):
        super(Fib1PinPointTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 30000.0))

    def runTest(self):
        self.runAndValidate()


class Fib1PinPointsTest(XIOSimTest):
    ''' End-to-end test with a single binary and multiple pinpoints.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        ppfile = os.path.join(self.xio.GetTreeDir(), "tests",
                              "fib..pintool.2.pp")
        self.xio.AddPinPointFile(ppfile)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "N.cfg"))

    def setUp(self):
        super(Fib1PinPointsTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 10100.0))

    def runTest(self):
        self.runAndValidate()

class ROITest(XIOSimTest):
    ''' End-to-end test with a single binary with ROI hooks.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("roi")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddROIOptions()
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "N.cfg"))

    def setUp(self):
        super(ROITest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 217000.0))

    def runTest(self):
        self.runAndValidate()

class ReplaceTest(XIOSimTest):
    ''' End-to-end test where we replace a function call with a NOP.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("repl")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddReplaceOptions("fib_repl")
        # 30K cycles for the magic instruction that replaces fib_repl()
        # 30K is high enough that the test can catch if we didn't add it,
        # yet low enough that it doesn't tickle the deadlock threshold (50K)
        repl = {
            "core_cfg.exec_cfg.exeu magic.latency" : "30000",
            "core_cfg.exec_cfg.exeu magic.rate" : "30000",
        }
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "config", "N.cfg"),
                                        repl)
        self.xio.AddZestoOptions(test_cfg)

    def setUp(self):
        super(ReplaceTest, self).setUp()
        # repl is ~1M instructions
        # when we correctly ignore the middle call, we expect ~450K
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 447000.0))
        # repl when we just ignore the middle call takes ~397K cycles + 30K magic
        self.expected_vals.append((xs.PerfStatRE("sim_cycle"), 426000.0))

    def runTest(self):
        self.runAndValidate()

class PowerTest(XIOSimTest):
    ''' End-to-end test that calculates power.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)

        repl = {
            "system_cfg.simulate_power" : "true"
        }
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "config", "A.cfg"),
                                        repl)
        self.xio.AddZestoOptions(test_cfg)

    def setUp(self):
        super(PowerTest, self).setUp()
        self.expected_vals.append((xs.PowerStatRE("  Runtime Dynamic"), 0.69))
        self.expected_vals.append((xs.PowerStatRE("  Total Leakage"), 0.48))

    def runTest(self):
        self.runAndValidate()

#TODO(skanev): Add a power trace test

class DFSTest(XIOSimTest):
    ''' End-to-end test that checks that "sample" DFS modifies frequencies.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("step")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)

        repl = {
            "system_cfg.simulate_power" : "true",
            "uncore_cfg.dvfs_cfg.config" : "\"sample\"",
            "uncore_cfg.dvfs_cfg.interval" : "20000",
        }
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "config", "A.cfg"),
                                        repl)
        self.xio.AddZestoOptions(test_cfg)

    def setUp(self):
        super(DFSTest, self).setUp()
        # Average freq ~901 MHz = 1600 * 2668273.0 / 4736546.0
        self.expected_vals.append((xs.PerfStatRE("c0.sim_cycle"), 2668273.0))
        self.expected_vals.append((xs.PerfStatRE("sim_cycle"), 4736546.0))
        # XXX: The dynamic number appears a little low, but that's more of a
        # validation issue, not a "hey, DFS is working" issue
        self.expected_vals.append((xs.PowerStatRE("  Runtime Dynamic"), 0.275))
        self.expected_vals.append((xs.PowerStatRE("  Total Leakage"), 0.48))

    def runTest(self):
        self.runAndValidate()

class Fib2Test(XIOSimTest):
    ''' End-to-end test with multiple programs running from start to end.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib", num_copies=2)
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "N.cfg"))

    def setUp(self):
        super(Fib2Test, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 236483.00))

    def runTest(self):
        self.runAndValidate()

class REPTest(XIOSimTest):
    ''' End-to-end test for REP instructions.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("rep")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(num_cores=1)
        self.xio.AddROIOptions()
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "none.cfg"))

    def setUp(self):
        super(REPTest, self).setUp()
        # 917,504 = 3.5 * 256 * 1024 -- see rep.cpp
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 917504.0))

    def runTest(self):
        self.runAndValidate()


if __name__ == "__main__":
    unittest.main()
