#!/usr/bin/env python

import re

STAT_THRESHOLD = 0.02

DECIMAL_RE = "-*\d+(\.\d*)?"

def PerfStatRE(stat):
    ''' Return a RE that looks for a XIOSim performance stat.'''
    return "^%s\s+(%s)" % (stat, DECIMAL_RE)

def PowerStatRE(stat):
    ''' Return a RE that looks for a McPAT power stat.'''
    return "^%s\s+=\s+(%s)" % (stat, DECIMAL_RE)

def PerfDistStatRE(stat):
    ''' Return a tuple of REs that looks for a particular label in a distribution.

    The stat is specified as dist_name[label], where label is the specifier of
    a bucket in the distribution. The specifier might be an index, a string, or
    some other value.

    Returns:
      (start_re, label_re, end_re), where start_re matches the beginning of the
      distribution's PDF printout, label_re matches the particular label of
      interest, and end_re matches the end of the PDF printout.
    '''

    m = re.match("^(.*)\[(.*)\]", stat)
    if m:
        stat_name = m.group(1)
        label = m.group(2)

    # The start and end labels are either start_hist or start_dist, depending
    # on the type of stat used.
    return ("^%s.start_[hd]ist" % stat_name,
            "^\s*%s\s+(%s)" % (label, DECIMAL_RE),
            "^%s.end_[hd]ist" % stat_name,
            )

def GetStat(fname, stat):
    ''' Find a stat value in a xiosim output file. '''
    if isinstance(stat, tuple):
        return GetDistStat(fname, stat)
    else:
        return GetScalarStat(fname, stat)

def GetScalarStat(fname, stat):
    ''' Find a scalar stat value in a xiosim output file.
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

def GetDistStat(fname, dist_stat_rxs):
    ''' Find a distribution stat value in a xiosim output file.

    Args:
        fname: output file name.
        dist_stat_rxs: tuple of regular expression looking for the start and
          end of the distribution and the label of the particular distribution
          bucket to match.  The value of interest is the counts of that
          particular bucket.

    Returns:
        Stat value, or NaN if not found.
    '''
    try:
        f = open(fname)
        val = float("NaN")
        start = dist_stat_rxs[0]
        label = dist_stat_rxs[1]
        end = dist_stat_rxs[2]
        start_re = re.compile(start)
        label_re = re.compile(label)
        end_re = re.compile(end)

        found_start = False
        for line in f:
            if not found_start:
                # First search for the beginning of the distribution.
                m = start_re.match(line)
                if m:
                    found_start = True
                    continue
            else:
                # Once found, then look for the label.
                m = label_re.match(line)
                if m:
                    val = float(m.group(1))
                    break
                # If we reach the end of the printout, then stop parsing, in
                # case we accidentally match this label from another
                # distribution.
                m = end_re.match(line)
                if m:
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
