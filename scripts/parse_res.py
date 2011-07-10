#! /usr/bin/python
import os.path

import spec

experimentDir = '/group/brooks/skanev/data_test1/ref'

def GetZestoStat(stat, amean):
    count = 0
    sum = 0

    for run in spec.runs:
        file = os.path.join(experimentDir, "%s.zesto" % run.name)
        print "%s: " % run.name,
        stat_val = ""
        for line in open(file):
            if stat in line:
#                stat_val = line.split()[1]
                stat_val = (line.split()[7]).replace(",","")
                count += 1
                sum += float(stat_val)
                break
        if stat_val == "":
            stat_val = "NOT FOUND!"
        print stat_val

    if (amean) and (count > 0):
        print "Mean: %f" % (sum / count)

def GetPinIPC():
    for run in spec.runs:
        cpi = 0
        pin_cpi = 0
        weight = 0
        cycles = 0
        file = os.path.join(experimentDir, "%s.zesto" % run.name)
        print "%s: " % run.name,
        stat_val = ""
        for line in open(file):
            if "n_insn" in line:
                stat_val = (line.split()[3]).replace(",","")
                weight = float(stat_val)
                stat_val = (line.split()[11])
                cycles = float(stat_val)
                stat_val = (line.split()[7]).replace(",","")
                cpi += cycles * weight / float(stat_val)
                stat_val = (line.split()[9]).replace(",","")
                pin_cpi += cycles * weight / float(stat_val)
        ipc = 1.0 / cpi
        pin_ipc = 1.0 / pin_cpi
        print "ipc: %f, pin_ipc : %f" % (ipc, pin_ipc)

if __name__ == "__main__":
#    GetZestoStat("sim_inst_rate", True)
    GetPinIPC()
                
