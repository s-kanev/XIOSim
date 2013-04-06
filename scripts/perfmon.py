#! /usr/bin/python
import os.path
import subprocess, shlex
import operator
import math

import scipy as sp
import scipy.stats.stats
import numpy as np

import spec

#Configuration params
PIN = "/home/skanev/pin/pin-2.8-36111-gcc.3.4.6-ia32_intel64-linux/ia32/bin/pinbin"
TOOL = "/home/skanev/harness/obj-ia32/harness.so"
FUNC_DIR = "/home/skanev/data_rtn_valid/ref1"
OUT_DIR = "/home/skanev/output"
ZESTO_OUT_DIR = "/home/skanev/data_atom_1MB_bkp"
#ZESTO_OUT_DIR = "/home/skanev/data_perf_BR/ref"

#Command-line builder and driver for the harness pintool
###########################################################
class PFMHarnessDriver(object):
    def __init__(self):
        self.cmd = "setarch i686 -3BL " + PIN + " -pause_tool 1 -injection child -separate_memory -t " + TOOL

    def AddFuncFile(self, fname):
        self.cmd += " -func_file " + fname

    def AddPoint(self, point_num):
        self.cmd += " -pnum " + str(point_num)

    def AddBmk(self, exe_dir, exe, args):
        exe = os.path.join(exe_dir, exe)
        self.cmd += " -- " + exe + " " + args

    def Exec(self, directory=".", redirin="", redirout="", redirerr=""):
        print "Executing: %s" % self.cmd

        if redirin != "":
            stdin = open(os.path.join(directory, redirin), "r")
        else:
            stdin = None

        if redirout != "":
            stdout = open(os.path.join(directory, redirout), "w")
        else:
            stdout = None

        if redirerr != "":
            stderr = open(os.path.join(directory, redirerr), "w")
        else:
            stderr = None

        child = subprocess.Popen(shlex.split(self.cmd), stdin=stdin, stdout=stdout, stderr=stderr, close_fds=True, cwd=directory)
        retcode = child.wait()

        if retcode == 0:
            print "Child completed successfully"
        else:
            print "Child failed! Error code: %d" % retcode

###########################################################
def RunPerfcount(bmk):
    func_file = os.path.join(FUNC_DIR, bmk.name +".func")
    npoints = sum(1 for line in open(func_file)) / 3

    for i in range(npoints):
        j = i+1
        pfm = PFMHarnessDriver()
        pfm.AddFuncFile(func_file)
        pfm.AddPoint(j)
        pfm.AddBmk(bmk.directory, bmk.executable, bmk.args)
        out_file = os.path.join(OUT_DIR, bmk.name + ".out."+str(j))
        err_file = os.path.join(OUT_DIR, bmk.name + ".err."+str(j))
        pfm.Exec(bmk.directory, redirin=bmk.input, redirout=out_file, redirerr=err_file)

###########################################################
def GetFuncNames(bmk, names):
    func_file = os.path.join(FUNC_DIR, bmk.name +".func")
    npoints = sum(1 for line in open(func_file)) / 3

    for i in range(npoints):
        j = i+1
        name = bmk.name + "." + str(j)
        names.append(name)
    return names

###########################################################
def GetFuncIns(bmk, res):
    func_file = os.path.join(FUNC_DIR, bmk.name +".func")
    i = 0
    start = 0
    diff = 0
    tmp_res = []
    for line in open(func_file):
        tokens = line.split()
        if i % 3 == 0:
            if len(tokens) < 4:
                tmp_res = []
                break
            start = int(tokens[3])
        elif i  % 3 == 1:
            if len(tokens) < 3:
                tmp_res = []
                break
            diff = int(tokens[2]) - start
            if diff < 1000000:
                diff = -1
            tmp_res.append(diff)
        i += 1
    res.extend(tmp_res)

###########################################################
def GetFuncWeights(bmk, res):
    func_file = os.path.join(FUNC_DIR, bmk.name +".func")
    i = 0
    for line in open(func_file):
        if i % 3 == 2:
            weigth = float(line.split()[0])
            res.append(weigth)
        i += 1

###########################################################
def GetZestoStat(bmk, stat, res):
    func_file = os.path.join(FUNC_DIR, bmk.name +".func")
    npoints = sum(1 for line in open(func_file)) / 3

    for i in range(npoints):
        j = i+1
        zesto_file = os.path.join(ZESTO_OUT_DIR, bmk.name + ".zesto.slice." + str(j))
        try:
            f = open(zesto_file)
            val = float("NaN")
            for line in f:
                if line.strip() == "":
                    continue
                tokens = line.split()
#                print tokens
                if stat == tokens[0]:
                    val = float(tokens[1])
                    break
        except IOError:
            val = float("NaN")
        res.append(val)

###########################################################
def GetZestoWeightedStat(bmk, stat, res):
    zesto_file = os.path.join(ZESTO_OUT_DIR, bmk.name + ".zesto")
    try:
        f = open(zesto_file)
        val = float("NaN")
        for line in f:
            if line.strip() == "":
                continue
            tokens = line.split()
#                print tokens
            if stat == tokens[0]:
                val = float(tokens[1])
                break
    except IOError:
        val = float("NaN")
    res.append(val)

###########################################################
def ParseCounter(bmk, counter, res, counter_out_dir):
    func_file = os.path.join(FUNC_DIR, bmk.name +".func")
    npoints = sum(1 for line in open(func_file)) / 3

    for i in range(npoints):
        stat_val = 0
        j = i+1
        err_file = os.path.join(counter_out_dir, bmk.name + ".err."+str(j))
#        print "%s.%d: " % (bmk.name, j),
        try:
            for line in  open(err_file):
                if counter in line:
                    stat_val = int(line.split()[1])
        except IOError:
            stat_val = 0
        res.append(stat_val)
#        print str(stat_val)

###########################################################
def DumpCounter(cnt, fmt, values):
    outfile = os.path.join(OUT_DIR, cnt)
    f = open(outfile, "w")
    for val in values:
        f.write(fmt % val)
    f.close()

###########################################################
def GetCounterRatio(cnt1, cnt2, name1, name2):
    ratio = []
    for curr1, curr2 in zip(cnt1, cnt2):
        try:
            curr_ratio = float(curr1) / float(curr2)
        except ZeroDivisionError:
            curr_ratio = float('NaN')
        # implies something wrong with collection
        if name1[1] == "inst" and curr1 < 10000000:
            curr_ratio = float('NaN')
        ratio.append(curr_ratio)
    return ratio

###########################################################
def AggregateCounters(name1, name2, name_ratio, counter_out_dir):
    cnt1 = []
    cnt2 = []
    ratio = []

    for run in spec.runs:
        ParseCounter(run, name1[0], cnt1, counter_out_dir)
        ParseCounter(run, name2[0], cnt2, counter_out_dir)

    ratio = GetCounterRatio(cnt1, cnt2, name1[1], name2[1])

    DumpCounter(name_ratio + "_names", "%s\n", names)
    DumpCounter(name1[1], "%d\n", cnt1)
    DumpCounter(name2[1], "%d\n", cnt2)
    DumpCounter(name_ratio, "%f\n", ratio)
    return cnt1, cnt2, ratio

###########################################################
def WeighCounter(name, out, counter_out_dir):

    for run in spec.runs:
        weights = []
        cnt = []
        ParseCounter(run, name, cnt, counter_out_dir)
        GetFuncWeights(run, weights)
#        print run.name, np.average(cnt, weights=weights)
        out.append(np.average(cnt, weights=weights))

###########################################################
def CalcCounterDelta(name, counter, orig_counter):
    delta = []
    delta_num = []
    avg_delta = 0.0
    geo_delta = 1.0
    cnt = 0
#    print orig_counter
    # calc counter difference
    for curr, orig in zip(counter, orig_counter):
        try:
            curr_delta = float(curr - orig) / float(orig)
        except ZeroDivisionError:
            curr_delta = float('NaN')
        if curr == -1 or orig == -1:
            curr_delta = float('NaN')
        # 100x difference implies sth wrong with measurement
        if abs(curr_delta) > 100:
            curr_delta = float('NaN')
        delta.append(curr_delta)
        if curr_delta == curr_delta:
            avg_delta += abs(curr_delta)
            geo_delta *= abs(curr_delta)
            cnt += 1
            delta_num.append(abs(curr_delta))

    print name, avg_delta / float(cnt), sp.stats.stats.gmean(delta_num), cnt
    DumpCounter("d_"+name, "%f\n", delta)
    return delta


###########################################################
if __name__ == "__main__":
#    RunPerfcount(spec.runs[1])
    names = []
    o_inst = []
    for run in spec.runs:
        GetFuncNames(run, names)
        GetFuncIns(run, o_inst)
    DumpCounter("o_inst", "%d\n", o_inst)

    zesto_ipc = []
    zesto_imiss = []
    zesto_l2miss = []
    zesto_dtlb2misses = []
    zesto_dl1lookup = []
    zesto_dir_MPKI = []
    zesto_addr_MPKI = []
    for run in spec.runs:
        GetZestoStat(run, "total_IPC", zesto_ipc)
        GetZestoStat(run, "c0.IL1.total_miss_rate", zesto_imiss)
        GetZestoStat(run, "LLC.total_miss_rate", zesto_l2miss)
        GetZestoStat(run, "c0.DTLB2.misses", zesto_dtlb2misses)
        GetZestoStat(run, "c0.DL1.total_lookups", zesto_dl1lookup)
        GetZestoStat(run, "c0.bpred_dir_MPKI", zesto_dir_MPKI)
        GetZestoStat(run, "c0.bpred_addr_MPKI", zesto_addr_MPKI)
    DumpCounter("zesto_ipc", "%f\n", zesto_ipc)
    DumpCounter("zesto_imiss", "%f\n", zesto_imiss)
    DumpCounter("zesto_l2miss", "%f\n", zesto_l2miss)
    DumpCounter("zesto_addr_MPKI", "%f\n", zesto_addr_MPKI)
    DumpCounter("zesto_dir_MPKI", "%f\n", zesto_dir_MPKI)

    # perfmon calculates tlb miss ratio as tlb misses / dl1 lookups
    ztlb_ratio = []
    for zmiss, zlook in zip(zesto_dtlb2misses, zesto_dl1lookup):
        try:
            ratio = float(zmiss) / float(zlook)
        except ZeroDivisionError:
            ratio = float("NaN")
        ztlb_ratio.append(ratio)
    DumpCounter("zesto_dtl2bmiss", "%f\n", ztlb_ratio)

    zesto_MPKI = []
    for aMPKI, dMPKI in zip(zesto_addr_MPKI, zesto_dir_MPKI):
        zesto_MPKI.append(aMPKI + dMPKI)
    DumpCounter("zesto_MPKI", "%f\n", zesto_MPKI)

    # Get per-slice stats
    (inst, cycles, ipc) = AggregateCounters(["PERF_COUNT_HW_INSTRUCTIONS", "inst"], ["PERF_COUNT_HW_CPU_CYCLES", "cycles"], "IPC", "/home/skanev/output_mordor_ipc")
    (imiss, iaccess, imiss_rate) = AggregateCounters(["PERF_COUNT_HW_CACHE_L1I:MISS", "imiss"], ["PERF_COUNT_HW_CACHE_L1I:ACCESS", "iaccess"], "il1_miss", "/home/skanev/output_il1")
    (l2miss, l2access, l2miss_rate) = AggregateCounters(["PERF_COUNT_HW_CACHE_LL:MISS", "l2miss"], ["PERF_COUNT_HW_CACHE_LL:ACCESS", "l2access"], "l2_miss", "/home/skanev/output_l2")
    (dtlbmiss, dtlbaccess, dtlbmiss_rate) = AggregateCounters(["PERF_COUNT_HW_CACHE_DTLB:MISS", "dtlbmiss"], ["PERF_COUNT_HW_CACHE_DTLB:ACCESS", "dtlbaccess"], "dtlb_miss", "/home/skanev/output_dtlb")
    (brmiss, bracceess, br_mpi) = AggregateCounters(["PERF_COUNT_HW_BRANCH_MISSES", "brmiss"], ["PERF_COUNT_HW_BRANCH_INSTRUCTIONS", "brinst"], "br_mpi", "/home/skanev/output_br")

    br_MPKI = []
    for mpi in br_mpi:
        br_MPKI.append(mpi * 1000)
    DumpCounter("br_MPKI", "%f\n", br_MPKI)

    # Get zesto weighted stats
    zesto_w_ipc = []
    zesto_w_imiss = []
    zesto_w_l2miss = []
    zesto_w_dtlb2misses = []
    zesto_w_dl1lookup = []
    zesto_w_dir_MPKI = []
    zesto_w_addr_MPKI = []
    for run in spec.runs:
        GetZestoWeightedStat(run, "total_IPC", zesto_w_ipc)
        GetZestoWeightedStat(run, "c0.IL1.total_miss_rate", zesto_w_imiss)
        GetZestoWeightedStat(run, "LLC.total_miss_rate", zesto_w_l2miss)
        GetZestoWeightedStat(run, "c0.DTLB2.misses", zesto_w_dtlb2misses)
        GetZestoWeightedStat(run, "c0.DL1.total_lookups", zesto_w_dl1lookup)
        GetZestoWeightedStat(run, "c0.bpred_dir_MPKI", zesto_w_dir_MPKI)
        GetZestoWeightedStat(run, "c0.bpred_addr_MPKI", zesto_w_addr_MPKI)

    # perfmon calculates tlb miss ratio as tlb misses / dl1 lookups
    ztlb_w_ratio = []
    for zmiss, zlook in zip(zesto_w_dtlb2misses, zesto_w_dl1lookup):
        try:
            ratio = float(zmiss) / float(zlook)
        except ZeroDivisionError:
            ratio = float("NaN")
        ztlb_w_ratio.append(ratio)
#    DumpCounter("zesto_dtl2bmiss", "%f\n", ztlb_w_ratio)

    zesto_w_MPKI = []
    for aMPKI, dMPKI in zip(zesto_w_addr_MPKI, zesto_w_dir_MPKI):
        zesto_w_MPKI.append(aMPKI + dMPKI)
#    DumpCounter("zesto_w_MPKI", "%f\n", zesto_w_MPKI)

    # Weighted IPC
    w_inst = []
    w_cycles = []
    WeighCounter("PERF_COUNT_HW_INSTRUCTIONS", w_inst, "/home/skanev/output_mordor_ipc")
    WeighCounter("PERF_COUNT_HW_CPU_CYCLES", w_cycles, "/home/skanev/output_mordor_ipc")
    w_ipc = GetCounterRatio(w_inst, w_cycles, "inst", "cycles")
    d_w_ipc = CalcCounterDelta("w_ipc", zesto_w_ipc, w_ipc)

    # Weighted IL1 misses
    w_l1i_access = []
    w_l1i_miss = []
    WeighCounter("PERF_COUNT_HW_CACHE_L1I:ACCESS", w_l1i_access, "/home/skanev/output_il1")
    WeighCounter("PERF_COUNT_HW_CACHE_L1I:MISS", w_l1i_miss, "/home/skanev/output_il1")
    w_imiss_rate = GetCounterRatio(w_l1i_miss, w_l1i_access, "imiss", "iaccess")
    d_w_ipc = CalcCounterDelta("w_imiss", zesto_w_imiss, w_imiss_rate)

    # Weighted L2 misses
    w_l2_access = []
    w_l2_miss = []
    WeighCounter("PERF_COUNT_HW_CACHE_LL:ACCESS", w_l2_access, "/home/skanev/output_l2")
    WeighCounter("PERF_COUNT_HW_CACHE_LL:MISS", w_l2_miss, "/home/skanev/output_l2")
    w_l2miss_rate = GetCounterRatio(w_l2_miss, w_l2_access, "l2miss", "l2access")
    d_w_l2miss = CalcCounterDelta("w_l2miss", zesto_w_l2miss, w_l2miss_rate)

    # Weighted TLB misses
    w_dtlb_access = []
    w_dtlb_miss = []
    WeighCounter("PERF_COUNT_HW_CACHE_DTLB:ACCESS", w_dtlb_access, "/home/skanev/output_dtlb")
    WeighCounter("PERF_COUNT_HW_CACHE_DTLB:MISS", w_dtlb_miss, "/home/skanev/output_dtlb")
    w_dtlbmiss_rate = GetCounterRatio(w_dtlb_miss, w_dtlb_access, "dtlbmiss", "dtlbaccess")
    d_w_dtlbmiss = CalcCounterDelta("w_dtlbmiss", ztlb_w_ratio, w_dtlbmiss_rate)

    # Weighted branch mkpi
    w_br_access = []
    w_br_miss = []
    WeighCounter("PERF_COUNT_HW_BRANCH_INSTRUCTIONS", w_br_access, "/home/skanev/output_br")
    WeighCounter("PERF_COUNT_HW_BRANCH_MISSES", w_br_miss, "/home/skanev/output_br")
    w_brmpi = GetCounterRatio(w_br_miss, w_br_access, "brmiss", "braccess")
    w_brmpki = []
    for w_mpi in w_brmpi:
        w_brmpki.append(w_mpi * 1000)
    d_w_brmpki = CalcCounterDelta("w_brmpki", zesto_w_MPKI, w_brmpki)


    inst_overhead = CalcCounterDelta("over_inst", inst, o_inst)
    d_ipc = CalcCounterDelta("IPC", zesto_ipc, ipc)
    d_imiss = CalcCounterDelta("imiss", zesto_imiss, imiss_rate) 
    d_l2miss = CalcCounterDelta("l2miss", zesto_l2miss, l2miss_rate) 
    d_dtlb2miss = CalcCounterDelta("dtlb2miss", ztlb_ratio, dtlbmiss_rate)
    d_daddrmiss = CalcCounterDelta("daddrmiss", zesto_addr_MPKI, br_MPKI)
    d_ddirmiss = CalcCounterDelta("ddirmiss", zesto_dir_MPKI, br_MPKI)
    d_brmiss = CalcCounterDelta("dbrmiss", zesto_MPKI, br_MPKI)
