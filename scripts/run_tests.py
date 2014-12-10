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
        self.setDriverParams()
        self.expected_vals = []


    def tearDown(self):
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

    def writeTestBmkConfig(self, bmk):
        ''' Create a temp benchmark config file in the test run directory.'''
        cfg_file = self.xio.GenerateTestConfig(bmk)
        bmk_cfg = os.path.join(self.run_dir, "%s.cfg" % bmk)
        with open(bmk_cfg, "w") as f:
            for l in cfg_file:
                f.write(l)
        return bmk_cfg


class Fib1Test(XIOSimTest):
    ''' End-to-end test with a single binary running from start to end.'''
    def setDriverParams(self):
        bmk_cfg = self.writeTestBmkConfig("fib")
        self.xio.AddBmks(bmk_cfg)

        self.xio.AddPinOptions()
        self.xio.AddPintoolOptions(1)
        self.xio.AddZestoOptions(os.path.join(self.xio.GetTreeDir(),
                                              "config", "Nconfuse.cfg"))

    def setUp(self):
        super(Fib1Test, self).setUp()
        self.expected_vals.append(("^all_insn", 118369.0))

    def runTest(self):
        self.runAndValidate()

if __name__ == "__main__":
    unittest.main()
