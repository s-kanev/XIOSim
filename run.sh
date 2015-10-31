#!/bin/bash

PIN=bazel-xiosim/external/pin/pin-2.14-67254-gcc.4.4.7-linux/pin.sh
BIN_PATH=bazel-bin/xiosim/pintool
PINTOOL=${BIN_PATH}/feeder_zesto.so
ZESTOCFG=xiosim/config/N.cfg
BENCHMARK_CFG_FILE=benchmarks.cfg

CMD_LINE="setarch x86_64 -R ${BIN_PATH}/harness \
                -benchmark_cfg $BENCHMARK_CFG_FILE  \
                -pin $PIN  \
                -pause_tool 1  \
                -xyzzy  \
                -catch_signals 0 \
                -t  \
                $PINTOOL  \
                -speculation false \
                -num_cores 1  \
                -s  \
                -config $ZESTOCFG"

echo $CMD_LINE
$CMD_LINE
