#! /bin/csh -f

#set PROGRAM = ../tests/fib
set PROGRAM = /home/skanev/ubench/fib
set PIN = /home/skanev/pin/pin-2.8-36111-gcc.3.4.6-ia32_intel64-linux/ia32/bin/pinbin
set PINTOOL = ./obj-ia32/feeder_zesto.so
set ZESTOCFG = ../config/A.cfg
set MEMCFG = ../dram-config/DDR2-800-5-5-5.cfg


set CMD_LINE = "setarch i686 -3BL $PIN -xyzzy -t $PINTOOL -maxins -1 -sanity -s -config $ZESTOCFG -config $MEMCFG -redir:sim tst.out -- $PROGRAM"
echo $CMD_LINE
$CMD_LINE
