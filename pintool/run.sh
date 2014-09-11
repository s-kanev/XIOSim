#! /bin/bash

PROGRAM=../tests/fib
PINHOME=/home/skanev/pin/2.14
PIN=${PINHOME}/pin.sh
PINTOOL=./obj-ia32/feeder_zesto.so
ZESTOCFG=../config/N.cfg
MEMCFG=../dram-config/DDR3-1600-9-9-9.cfg

CMD_LINE="setarch i686 -3BL ${PIN} -pause_tool 1 -injection child -xyzzy -t ${PINTOOL} -sanity -pipeline_instrumentation -s -config $ZESTOCFG -config $MEMCFG -redir:sim sim.out -power true -power:rtp_interval 10000 -power:rtp_file sim.power -dvfs:interval 40000 -- $PROGRAM"

echo ${CMD_LINE}
${CMD_LINE}
