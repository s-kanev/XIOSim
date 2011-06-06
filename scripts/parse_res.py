#! /usr/bin/python
import os.path

import spec

experimentDir = '/group/brooks/skanev/data_atom/ref'

def GetZestoStat(stat, amean):
    count = 0
    sum = 0

    for run in spec.runs:
        file = os.path.join(experimentDir, "%s.zesto" % run.name)
        print "%s: " % run.name,
        stat_val = ""
        for line in open(file):
            if stat in line:
                stat_val = line.split()[1]
                count += 1
                sum += float(stat_val)
                break
        if stat_val == "":
            stat_val = "NOT FOUND!"
        print stat_val

    if (amean) and (count > 0):
        print "Mean: %f" % (sum / count)

if __name__ == "__main__":
    GetZestoStat("sim_inst_rate", True)
                
