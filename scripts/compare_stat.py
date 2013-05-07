#! /usr/bin/python

import os
import re

RUN1 = '/group/brooks/skanev/data_test/ref'
RUN2 = '/group/brooks/skanev/data_test/ref_bkp'
STAT = 'total_IPC'


def GetZestoStat(file, stat):
    try:
        f = open(file)
        val = float("NaN")
        rx = re.compile(stat)

        for line in f:
            m = rx.match(line)
            if m:
                val = float(line.split()[1])
                break
    except IOError:
        val = float("NaN")
    return val

def GetRunStats(run, stat):
    stats = {}

    res = os.listdir(run)
    for f in res:
        if not "slice" in f:
            continue

        stats[f] = GetZestoStat("%s/%s" % (run, f), stat)

    return stats

if __name__ == "__main__":
    stats1 = GetRunStats(RUN1, STAT)
    stats2 = GetRunStats(RUN2, STAT)

    bmks = []

    for bmk in stats1.keys():
        if bmk in stats2:
            bmks.append(bmk)

    bmks.sort()

    for bmk in bmks:
        delta = (stats1[bmk] - stats2[bmk]) / stats2[bmk] * 100
        print "%s %.2f" % (bmk, delta)
