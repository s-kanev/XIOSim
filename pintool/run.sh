#! /bin/csh -f

set PROGRAM = /home/skanev/pin/zpin/tests/fib
#set PROGRAM = /home/skanev/ubench/fib
set PIN = /home/skanev/pin/pin-2.8-36111-gcc.3.4.6-ia32_intel64-linux/ia32/bin/pinbin
set PINTOOL = ./obj-ia32/feeder_zesto.so
set ZESTOCFG = ../config/M.cfg


set CMD_LINE = "setarch i686 -3BL $PIN -xyzzy -t $PINTOOL -maxins -1 -s -config $ZESTOCFG -redir:sim tst.out -- $PROGRAM"
echo $CMD_LINE
$CMD_LINE
