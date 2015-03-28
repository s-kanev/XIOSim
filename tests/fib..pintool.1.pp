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
# C: sum:dummy Command: /home/skanev/pin/2.12/ia32/bin/pinbin -p64 /home/skanev/pin/2.12/intel64/bin/pinbin -t /home/skanev/pin/2.12/source/tools/PinPoints/obj-ia32/isimpoint.so -slice_size 30000 -o fib. -- ./fib

#Pinpoint= 1 Slice= 1  Icount= 30000  Len= 30000
region 1 100.000 1 1 30000 30000
slice 1 0 0.100336
warmup_factor 0 
start 1 514  0
end   2 1004 0

markedInstrs 2
mark 1 0x8048213 1 0 0
mark 2 0x80481f1 0 1 0

totalIcount 117923
pinpoints 1
endp
