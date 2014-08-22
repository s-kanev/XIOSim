#! /bin/bash

PROGRAM=../tests/fib
PINHOME=/home/skanev/pin/2.12
PIN=$PINHOME/pin.sh
PINTOOL=./obj-ia32/feeder_zesto.so
ZESTOCFG=../config/Nconfuse.cfg
MEMCFG=../dram-config/DDR3-1600-9-9-9.cfg
BENCHMARK_CFG_FILE=benchmarks.cfg

export LD_LIBRARY_PATH=/home/skanev/lib

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
