#! /bin/csh -f

#set PROGRAM = /home/skanev/pin/zpin/tests/fib
#set PROGRAM = /home/skanev/ubench/fib
#set PROGRAM = /home/skanev/ubench/test/fstsw
set PROGRAM_DIR = /home/skanev/ubench/fp
#set PROGRAM_DIR = /group/brooks/skanev/cpu2006/benchspec/CPU2006/416.gamess/run/run_base_ref_O3gcc4static241.0000
#set PROGRAM = ../../exe/gamess_base.O3gcc4static241
set PROGRAM = ./accum
#set FFWD = 822800000000 
set FFWD = 0

set PIN = /home/skanev/pin/pin-2.8-36111-gcc.3.4.6-ia32_intel64-linux/ia32/bin/pinbin
set PINTOOL = /home/skanev/pin/zpin/pintool/obj-ia32/feeder_zesto.so
set ZESTOCFG = /home/skanev/pin/zpin/config/M.cfg
set CMD_LINE = "setarch i686 -3BL $PIN -pause_tool 15 -t $PINTOOL -ffwd $FFWD -s -config $ZESTOCFG -redir:sim tst.out -- $PROGRAM"
#echo $CMD_LINE
limit coredumpsize unlimited
cd $PROGRAM_DIR
$CMD_LINE | cat
