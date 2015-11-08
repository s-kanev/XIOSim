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
# C: sum:dummy Command: /home/skanev/pin/2.12/intel64/bin/pinbin -p32 /home/skanev/pin/2.12/ia32/bin/pinbin -waiting_process 22726 -waiting_injector 22733 -sigchld_handler 1 -t /home/skanev/pin/2.12/source/tools/PinPoints/obj-intel64/isimpoint.so -slice_size 30000 -o fib. -- ./fib

#Pinpoint= 1 Slice= 1  Icount= 30000  Len= 30000
region 1 100.000 1 1 30000 30000
slice 1 0 0.136894
warmup_factor 0 
start 1 11  0
end   2 990 0

markedInstrs 2
mark 1 0x4492dd 1 0 0
mark 2 0x400e2e 0 1 0

totalIcount 116180
pinpoints 1
endp
