#! /usr/bin/python

#Command lines for different perf counter data gathered
def CountCLK():
    cmd = "en='CPU_CLK_UNHALTED.CORE' "
    return cmd

def CountINST():
   cmd = "en='INST_RETIRED.ANY' "
   return cmd
