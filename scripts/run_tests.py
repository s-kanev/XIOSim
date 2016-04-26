#!/usr/bin/env python

import copy
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

import xiosim_driver as xd
import xiosim_stat as xs


def CreateDriver(bazel_env):
    if bazel_env:
        TEST_DIR = os.environ["TEST_SRCDIR"]
        XIOSIM_INSTALL = TEST_DIR
        XIOSIM_TREE = TEST_DIR
        # the bazel sandbox doesn't have /dev/shm mounted
        BRIDGE_DIRS = "/tmp/"
    else:
        XIOSIM_INSTALL = os.environ["XIOSIM_INSTALL"]
        XIOSIM_TREE = os.environ["XIOSIM_TREE"]
        BRIDGE_DIRS = ""

    if "TARGET_ARCH" in os.environ:
        ARCH = os.environ["TARGET_ARCH"]
    else:
        # TODO(skanev): figure out how to plumb through bazel's cpu parameter
        ARCH = "k8"
    xio = xd.XIOSimDriver(XIOSIM_INSTALL, XIOSIM_TREE, ARCH, bridge_dirs=BRIDGE_DIRS)
    return xio


class XIOSimTest(unittest.TestCase):
    ''' Test fixtures for XIOSim end-to-end tests.'''
    def setUp(self):
        ''' Set up a driver with common options and a temp run directory.'''
        # Running under bazel, we don't need no other environment
        bazel_env = ("TEST_SRCDIR" in os.environ)
        self.xio = CreateDriver(bazel_env)
        if bazel_env:
            self.run_dir = os.environ["TEST_TMPDIR"]
        else:
            self.run_dir = tempfile.mkdtemp()
        self.clean_run_dir = ("LEAVE_TEST_DIR" not in os.environ) and not bazel_env
        self.setDriverParams()
        self.expected_vals = []
        self.expected_exprs = []
        self.bmk_expected_vals = []
        self.background_bmk = None

    def tearDown(self):
        if self.clean_run_dir:
            shutil.rmtree(self.run_dir)
        if self.background_bmk:
            self.background_bmk.kill()


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

        for re, golden_val in self.bmk_expected_vals:
            val = xs.GetStat(self.xio.GetTestOut(), re)
            res = xs.ValidateStat(val, golden_val)
            self.assertEqual(res, True, "%s: expected %.6f, got %.6f" %
                                        (re, golden_val, val))

        for re, expr in self.expected_exprs:
            val = xs.GetStat(self.xio.GetSimOut(), re)
            test_fn = expr[0]
            test_val = expr[1]
            test_fn(val, test_val, "%s: %s %.2f, got %.2f" %
                                   (re, test_fn.__name__, test_val, val))

    def writeTestBmkConfig(self, bmk, num_copies=1):
        ''' Create a temp benchmark config file in the test run directory. '''
        cfg_file = self.xio.GenerateTestBmkConfig(bmk, num_copies)
        bmk_cfg = os.path.join(self.run_dir, "%s.cfg" % bmk)
        with open(bmk_cfg, "w") as f:
            for l in cfg_file:
                f.write(l)
        return bmk_cfg

    def writeTestAttachBmkConfig(self, bmk, pid):
        ''' Create a temp benchmark config file for attaching in the test
        run directory. '''
        cfg_file = self.xio.GenerateTestAttachBmkConfig(bmk, pid)
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

    def startBackgroundBenchmark(self, bmk):
        ''' Start a benchmark run in the background that we can attach to
        later. '''
        bmk_bin = self.xio.GetTestBin(bmk)
        child = subprocess.Popen(bmk_bin, cwd=self.run_dir)
        self.background_bmk = child
        return child.pid


class Fib1Test(XIOSimTest):
    ''' End-to-end test with a single binary running from start to end.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()

    def setUp(self):
        super(Fib1Test, self).setUp()
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 112176.00))
        else:
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 114495.0))

    def runTest(self):
        self.runAndValidate()


class NoneTest(XIOSimTest):
    ''' End-to-end test with the 'none' core model.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "none.cfg"))
        self.xio.AddPinOptions()

    def setUp(self):
        super(NoneTest, self).setUp()
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 112176.00))
        else:
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 114495.0))

    def runTest(self):
        self.runAndValidate()


class Fib1LengthTest(XIOSimTest):
    ''' End-to-end test with a single binary running for X instructions.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddInstLength(10000)

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

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddSkipInst(50000)

    def setUp(self):
        super(Fib1SkipTest, self).setUp()
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 62176.00))
        else:
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 64495.0))

    def runTest(self):
        self.runAndValidate()


class Fib1PinPointTest(XIOSimTest):
    ''' End-to-end test with a single binary and one pinpoint.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        ppfile = os.path.join(self.xio.GetTreeDir(), "tests",
                              self.xio.TARGET_ARCH, "fib..pintool.1.pp")
        self.xio.AddPinPointFile(ppfile)

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

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        ppfile = os.path.join(self.xio.GetTreeDir(), "tests",
                              self.xio.TARGET_ARCH, "fib..pintool.2.pp")
        self.xio.AddPinPointFile(ppfile)

    def setUp(self):
        super(Fib1PinPointsTest, self).setUp()
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 10000.0))
        else:
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 9430.0))

    def runTest(self):
        self.runAndValidate()

class ROITest(XIOSimTest):
    ''' End-to-end test with a single binary with ROI hooks.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("roi")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddROIOptions()

    def setUp(self):
        super(ROITest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 90000.0))

    def runTest(self):
        self.runAndValidate()

class ReplaceTest(XIOSimTest):
    ''' End-to-end test where we replace a function call with a NOP.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("repl")
        self.xio.AddBmks(bmk_cfg)

        # 30K cycles for the magic instruction that replaces fib_repl()
        # 30K is high enough that the test can catch if we didn't add it,
        # yet low enough that it doesn't tickle the deadlock threshold (50K)
        repl = {
            "core_cfg.exec_cfg.exeu magic.latency" : "30000",
            "core_cfg.exec_cfg.exeu magic.rate" : "30000",
            "system_cfg.ignore_cfg.funcs" : "{\"fib_repl\"}",
        }
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "xiosim/config", "N.cfg"),
                                        repl)
        self.xio.AddConfigFile(test_cfg)
        self.xio.AddPinOptions()
        self.xio.AddIgnoreOptions()

    def setUp(self):
        super(ReplaceTest, self).setUp()
        # repl is ~1M instructions
        # when we correctly ignore the middle call, we expect ~466K
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 466283.00))
        else:
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 439975.00))
        # repl when we just ignore the middle call takes ~550K cycles + 30K magic
        # XXX: Disable this check for now. Branch NOP-iness makes this flaky
        # self.expected_vals.append((xs.PerfStatRE("sim_cycle"), 580000.0))

    def runTest(self):
        self.runAndValidate()

class PowerTest(XIOSimTest):
    ''' End-to-end test that calculates power.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        repl = {
            "system_cfg.simulate_power" : "true"
        }
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "xiosim/config", "A.cfg"),
                                        repl)
        self.xio.AddConfigFile(test_cfg)
        self.xio.AddPinOptions()


    def setUp(self):
        super(PowerTest, self).setUp()
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PowerStatRE("  Runtime Dynamic"), 0.53))
        else:
            self.expected_vals.append((xs.PowerStatRE("  Runtime Dynamic"), 0.589))
        self.expected_vals.append((xs.PowerStatRE("  Total Leakage"), 0.48))

    def runTest(self):
        self.runAndValidate()

#TODO(skanev): Add a power trace test

class DFSTest(XIOSimTest):
    ''' End-to-end test that checks that "sample" DFS modifies frequencies.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("step")
        self.xio.AddBmks(bmk_cfg)

        repl = {
            "system_cfg.simulate_power" : "true",
            "system_cfg.dvfs_cfg.config" : "\"sample\"",
            "system_cfg.dvfs_cfg.interval" : "20000",
        }
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "xiosim/config", "A.cfg"),
                                        repl)
        self.xio.AddConfigFile(test_cfg)
        self.xio.AddPinOptions()


    def setUp(self):
        super(DFSTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("c0.effective_frequency"), 901.0))
        # XXX: The dynamic number appears a little low, but that's more of a
        # validation issue, not a "hey, DFS is working" issue
        self.expected_vals.append((xs.PowerStatRE("  Runtime Dynamic"), 0.263))
        self.expected_vals.append((xs.PowerStatRE("  Total Leakage"), 0.48))

    def runTest(self):
        self.runAndValidate()

class Fib2Test(XIOSimTest):
    ''' End-to-end test with multiple programs running from start to end.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib", num_copies=2)
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()

    def setUp(self):
        super(Fib2Test, self).setUp()
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 224352.00))
        else:
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 228990.0))

    def runTest(self):
        self.runAndValidate()

class REPTest(XIOSimTest):
    ''' End-to-end test for REP instructions.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("rep")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "none.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddROIOptions()

    def setUp(self):
        super(REPTest, self).setUp()
        # 917,504 = 3.5 * 256 * 1024 -- see rep.cpp
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 917504.0))

    def runTest(self):
        self.runAndValidate()

class SegfTest(XIOSimTest):
    ''' End-to-end test for faults on speculative path.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("segf")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()

    def setUp(self):
        super(SegfTest, self).setUp()

    def runTest(self):
        self.runAndValidate()

class ChaseTest(XIOSimTest):
    ''' Pointer chase test.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("chase")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddROIOptions()

    def setUp(self):
        super(ChaseTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 1310720))
        # Random tests are random. Give them *very* loose bounds.
        self.expected_exprs.append((xs.PerfStatRE("total_IPC"),
                                    (self.assertLess, 0.3)))

    def runTest(self):
        self.runAndValidate()

class PrefetchTest(XIOSimTest):
    ''' Pointer chase test with a SW prefetch.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("prefetch")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddROIOptions()

    def setUp(self):
        super(PrefetchTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 2621440))
        # Random tests are random. Give them *very* loose bounds.
        self.expected_exprs.append((xs.PerfStatRE("total_IPC"),
                                    (self.assertGreater, 0.9)))

    def runTest(self):
        self.runAndValidate()

class IgnorePCTest(XIOSimTest):
    ''' End-to-end test where we ignore specific instructions.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("ignore")
        self.xio.AddBmks(bmk_cfg)

        if self.xio.TARGET_ARCH == "k8":
            repl = {"system_cfg.ignore_cfg.pcs" : "{\"main+0x20\"}"}
        else:
            repl = {"system_cfg.ignore_cfg.pcs" : "{\"main+0x17\"}"}
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "xiosim/config", "none.cfg"),
                                        repl)
        self.xio.AddConfigFile(test_cfg)
        self.xio.AddPinOptions()
        self.xio.AddROIOptions()
        self.xio.AddIgnoreOptions()

    def setUp(self):
        super(IgnorePCTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 30000.0))

    def runTest(self):
        self.runAndValidate()

class TimeTest(XIOSimTest):
    ''' End-to-end test for gettimeofday(). '''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("time")
        self.xio.AddBmks(bmk_cfg)
        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddROIOptions()

    def setUp(self):
        super(TimeTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 3000000.0))

        # Set up expected output from the simulated program.
        # 1M cycles / 3.2GHz = 317us.
        elapsed_re = "Elapsed: (%s) sec" % xs.DECIMAL_RE
        self.bmk_expected_vals.append((elapsed_re, 0.000317))

    def runTest(self):
        self.runAndValidate()

class TimeVDSOTest(XIOSimTest):
    ''' End-to-end test for __vdso_gettimeofday(). '''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("time")
        self.xio.AddBmks(bmk_cfg)
        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddROIOptions()
        self.xio.AddTraceFile("trace.out")

    def setUp(self):
        super(TimeVDSOTest, self).setUp()
        if self.xio.TARGET_ARCH == "piii":
            self.tearDown()
            self.skipTest("VDSO not used for gettimeofday() on i686")
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 3000000.0))

        # Set up expected output from the simulated program.
        # 1M cycles / 3.2GHz = 317us.
        elapsed_re = "Elapsed: (%s) sec" % xs.DECIMAL_RE
        self.bmk_expected_vals.append((elapsed_re, 0.000317))

    def runTest(self):
        self.runAndValidate()

#TODO(skanev): Re-enable after fixing XIOSIM-32
@unittest.skip("Flaky under load. Fix first.")
class AttachTest(XIOSimTest):
    ''' End-to-end test for attaching to a running process. '''
    def setDriverParams(self):
        # will get created later, when we have a pid
        bmk_cfg = os.path.join(self.run_dir, "loop.cfg")
        self.xio.AddBmks(bmk_cfg)
        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "none.cfg"))
        self.xio.AddPinOptions()
        self.xio.AddInstLength(10000)

    def setUp(self):
        super(AttachTest, self).setUp()
        self.expected_vals.append((xs.PerfStatRE("all_insn"), 10000.0))

    def runTest(self):
        pid = self.startBackgroundBenchmark("loop")
        self.writeTestAttachBmkConfig("loop", pid)

class RdtscTest(XIOSimTest):
    ''' End-to-end test for rdtsc virtualization. '''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("rdtsc")
        self.xio.AddBmks(bmk_cfg)
        self.xio.AddConfigFile(os.path.join(self.xio.GetTreeDir(),
                                              "xiosim/config", "N.cfg"))
        self.xio.AddPinOptions()

    def setUp(self):
        super(RdtscTest, self).setUp()
        high_re = "High difference: (%s)" % xs.DECIMAL_RE
        low_re = "Low difference: (%s)" % xs.DECIMAL_RE
        self.bmk_expected_vals.append((high_re, 0))
        self.bmk_expected_vals.append((low_re, 100000))

    def runTest(self):
        self.runAndValidate()

class Fib2CoreTest(XIOSimTest):
    ''' End-to-end test with multiple programs running on 2 cores.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib", num_copies=2)
        self.xio.AddBmks(bmk_cfg)

        repl = {
            "system_cfg.num_cores" : "2",
        }
        test_cfg = self.writeTestConfig(os.path.join(self.xio.GetTreeDir(),
                                                     "xiosim/config", "H.cfg"),
                                        repl)
        self.xio.AddConfigFile(test_cfg)

        self.xio.AddPinOptions()

    def setUp(self):
        super(Fib2CoreTest, self).setUp()
        if self.xio.TARGET_ARCH == "k8":
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 224352.00))
        else:
            self.expected_vals.append((xs.PerfStatRE("all_insn"), 228990.0))

    def runTest(self):
        self.runAndValidate()

if __name__ == "__main__":
    unittest.main()
