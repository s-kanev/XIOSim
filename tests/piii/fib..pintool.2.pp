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
# C: sum:dummy Command: /home/skanev/pin/2.12/ia32/bin/pinbin -p64 /home/skanev/pin/2.12/intel64/bin/pinbin -t /home/skanev/pin/2.12/source/tools/PinPoints/obj-ia32/isimpoint.so -slice_size 10000 -o fib. -- ./fib

#Pinpoint= 1 Slice= 0  Icount= 0  Len= 10000
region 1 16.667 1 1 10000 0
slice 0 1 0.175846
warmup_factor 0 
start 1 1  0
end   2 5 0

#Pinpoint= 2 Slice= 5  Icount= 50000  Len= 10000
region 2 83.333 1 1 10000 50000
slice 5 0 0.020583
warmup_factor 0 
start 3 1506  0
end   3 2000 0

markedInstrs 3
mark 1 0x80480f0 1 0 0
mark 2 0x8057e0a 0 1 0
mark 3 0x8048213 1 1 0

totalIcount 117923
pinpoints 2
endp
