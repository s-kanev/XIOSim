#
# Format:
#   A region tag must preceed its start and end tags with no other
#   tags in between:
#     region <region-id> <weight> <n-start-points> <n-end-points> <length> <icount-from-program-start>
#     slice <slice-id> <phase-id> <distance-from-centroid>
#     start <marked-instr-id> <execution-count-since-program-start> <icount-from-slice-start>
#     end   <marked-instr-id> <execution-count-since-program-start> <icount-from-slice-start>
#
#   A markedInstrs tag must preceed the mark tags.  There must be
#   exactly one markedInstrs tag in the file.
#     markedInstrs <n-marked-instrs>
#     mark <mark-id> <instr-address> <used-in-start> <used-in-end> <is-nop-instr>
#
# I: 0


thread 0
version 3
# C: sum:dummy Command: /home/skanev/pin/2.12/intel64/bin/pinbin -p32 /home/skanev/pin/2.12/ia32/bin/pinbin -waiting_process 23030 -waiting_injector 23037 -sigchld_handler 1 -t /home/skanev/pin/2.12/source/tools/PinPoints/obj-intel64/isimpoint.so -slice_size 10000 -o fib. -- ./fib

#Pinpoint= 1 Slice= 1  Icount= 10000  Len= 10000
region 1 16.667 1 1 10000 10000
slice 1 1 0.233842
warmup_factor 0 
start 1 1  0
end   2 15 0

#Pinpoint= 2 Slice= 4  Icount= 40000  Len= 10000
region 2 83.333 1 1 10000 40000
slice 4 0 0.006894
warmup_factor 0 
start 3 955  0
end   4 1482 0

markedInstrs 4
mark 1 0x40108f 1 0 0
mark 2 0x44c404 0 1 0
mark 3 0x400e3b 1 0 0
mark 4 0x400e0e 0 1 0

totalIcount 116180
pinpoints 2
endp
