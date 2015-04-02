#!/usr/bin/env python

import re

STAT_THRESHOLD = 0.01

DECIMAL_RE = "-*\d+(\.\d*)?"

def PerfStatRE(stat):
    ''' Return a RE that looks for a XIOSim performance stat.'''
    return "^%s\s+(%s)" % (stat, DECIMAL_RE)


def PowerStatRE(stat):
    ''' Return a RE that looks for a McPAT power stat.'''
    return "^%s\s+=\s+(%s)" % (stat, DECIMAL_RE)


def GetStat(fname, stat):
    ''' Find a stat value in a xiosim output file.
        fname: output file name.
        stat: regular expression, looking for the stat. The first group
              in the RE is the stat of interest.
    Returns:
        Stat value, or NaN if not found.
    '''
    try:
        f = open(fname)
        val = float("NaN")
        rx = re.compile(stat)

        for line in f:
            m = rx.match(line)
            if m:
                val = float(m.group(1))
                break
    except IOError:
        val = float("NaN")
    return val


def ValidateStat(val, golden):
    ''' Check whether stat value matches a golden one.

    Returns:
        True, if within pre-set threshold (STAT_THRESHOLD).
    '''
    if val == None:
        return False

    if golden == 0.0:
        return (val == 0.0)

    abs_err = abs(val - golden) / golden
    return (abs_err <= STAT_THRESHOLD)
