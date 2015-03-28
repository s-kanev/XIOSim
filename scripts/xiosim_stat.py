#!/usr/bin/env python

import re

STAT_THRESHOLD = 0.01


def GetStat(fname, stat):
    ''' Find a stat value in a xiosim output file.

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
                val = float(line.split()[1])
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
