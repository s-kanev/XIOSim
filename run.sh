#!/bin/bash

PIN_PATH=bazel-bin/external/pin
export PIN_VM_LD_LIBRARY_PATH=${PIN_PATH}
PIN=${PIN_PATH}/pinbin

BIN_PATH=bazel-bin/xiosim/pintool
PINTOOL=${BIN_PATH}/feeder_zesto.so
TIMING_SIM=${BIN_PATH}/timing_sim
ZESTOCFG=xiosim/config/N.cfg
BENCHMARK_CFG_FILE=benchmarks.cfg

CMD_LINE="setarch x86_64 -R ${BIN_PATH}/harness \
                -benchmark_cfg $BENCHMARK_CFG_FILE  \
                -timing_sim ${TIMING_SIM} \
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
