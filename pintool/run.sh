#!/bin/bash

PIN=${PIN_ROOT}/pin.sh
PINTOOL=./obj-ia32/feeder_zesto.so
ZESTOCFG=../config/Nconfuse.cfg
BENCHMARK_CFG_FILE=benchmarks.cfg

CMD_LINE="setarch i686 -BR ./obj-ia32/harness \
                -benchmark_cfg $BENCHMARK_CFG_FILE  \
                -pin $PIN  \
                -pause_tool 1  \
                -xyzzy  \
                -t  \
                $PINTOOL  \
                -num_cores 1  \
                -s  \
                -config $ZESTOCFG"

echo $CMD_LINE
$CMD_LINE
