# Makefile - simulator suite make file
#
# Please see copyright/licensing notices at the end of this file

##################################################################
#
# Modify the following definitions to suit your build environment,
# NOTE: most platforms should not require any changes
#
##################################################################

##
## vanilla Unix, GCC build
##
## tested hosts:
##
## Redhat Enterprise 5/Linux 2.6.18/64-bit
## Redhat Enterprise 4/Linux 2.6.9/32-bit
##
CC = g++

##################################################################
# Uncomment only one of the following OFLAGS, or make your own

# For debug:
OFLAGS = -O0 -g -m32 -DMIN_SYSCALL_MODE -DUSE_SSE_MOVE -Wall -DDEBUG -msse4a -mfpmath=sse
OFLAGS_SAFE = $(OFLAGS)

# Fully-optimized, but with profiling for gprof:
#OFLAGS = -O3 -g -pg -m32 -DMIN_SYSCALL_MODE -DUSE_SSE_MOVE -Wall -static -fexpensive-optimizations -mtune=core2 -march=core2 -msse4a -mfpmath=sse -funroll-loops
# Fully-optimized:
#OFLAGS = -O3 -m32 -DMIN_SYSCALL_MODE -DUSE_SSE_MOVE -Wall -static -fexpensive-optimizations -mtune=core2 -march=core2 -msse4a -mfpmath=sse -funroll-loops -Wuninitialized

#Needed only by syscall.c because > O0 breaks it
#OFLAGS_SAFE = -O0 -g -pg -m32 -DMIN_SYSCALL_MODE -DUSE_SSE_MOVE -Wall -static -mfpmath=sse -msse4a


##################################################################
# Uncomment to turn on pipeline event logging 
ZTRACE = #-DZTRACE

##################################################################
MFLAGS = `./sysprobe -flags`
MLIBS  = `./sysprobe -libs` -lm
ENDIAN = `./sysprobe -s`
MAKE = make
AR = ar qcv
AROPT =
RANLIB = ranlib
RM = rm -f
RMDIR = rm -f
LN = ln -s
LNDIR = ln -s
DIFF = diff
OEXT = o
LEXT = a
EEXT =
CS = ;
X=/

# Compilation-specific feature flags
#
# -DDEBUG	- turns on debugging features
# -DGZIP_PATH	- specifies path to GZIP executable, only needed if SYSPROBE
#		  cannot locate binary
# -DSLOW_SHIFTS	- emulate all shift operations, only used for testing as
#		  sysprobe will auto-detect if host can use fast shifts
# -DLINUX_RHEL4 - we needed to use this for RHEL4, but not for RHEL5 (default)
#
#FFLAGS = -DLINUX_RHEL4

##################################################################
#
# complete flags
#
CFLAGS = $(MFLAGS) $(FFLAGS) $(OFLAGS) $(BINUTILS_INC) $(BINUTILS_LIB) $(ZTRACE)
CFLAGS_SAFE = $(MFLAGS) $(FFLAGS) $(OFLAGS_SAFE) $(BINUTILS_INC) $(BINUTILS_LIB) $(ZTRACE)
SLAVE_CFLAGS = -DZESTO_PIN $(CFLAGS)

#
# all the sources
#
SRCS =  \
bbtracker.c          eio.c                endian.c              eval.c             \
loader.c	     machine.c            main.c                memory.c           \
misc.c               options.c            range.c               regs.c             \
sim-eio.c            sim-fast.c           stats.c               symbol.c           \
syscall.c	     sysprobe.c           sim-cache.c           slave.c	           \
loader.c             symbol.c             syscall.c             sim-main.c         \
callbacks.c       slices.cpp

HDRS = \
bbtracker.h          cache.h                                    thread.h           \
eio.h                endian.h             eval.h                eventq.h           \
host.h               loader.h             machine.h             memory.h           \
mem-system.h         misc.h               options.h             ptrace.h           \
range.h              regs.h               resource.h            sim.h              \
stats.h              symbol.h             syscall.h             version.h          \
machine.def          elf.h                x86flow.def           interface.h        \
callbacks.h

OBJS_NOMAIN =	\
endian.$(OEXT)       eval.$(OEXT)         \
machine.$(OEXT)      memory.$(OEXT)       misc.$(OEXT)          options.$(OEXT)    \
range.$(OEXT)        regs.$(OEXT)         stats.$(OEXT)         symbol.$(OEXT)     \
sim-main.$(OEXT)     slices.$(OEXT)

OBJS = main.$(OEXT) eio.$(OEXT) loader.$(OEXT) $(OBJS_NOMAIN) syscall.$(OEXT) 
OBJS_SLAVE = callbacks.$(OEXT) slave.$(OEXT) loader.$(OEXT) $(OBJS_NOMAIN)

# Zesto specific files
ZSRCS = \
sim-zesto.cpp zesto-core.cpp zesto-opts.c zesto-oracle.cpp zesto-fetch.cpp         \
zesto-decode.cpp zesto-alloc.cpp zesto-exec.cpp zesto-commit.cpp zesto-cache.cpp   \
zesto-dram.cpp zesto-bpred.cpp zesto-memdep.cpp zesto-prefetch.cpp                 \
zesto-uncore.cpp zesto-MC.cpp zesto-dumps.cpp

ZHDRS = \
zesto-structs.h zesto-core.h zesto-opts.h zesto-oracle.h zesto-fetch.h             \
zesto-decode.h zesto-alloc.h zesto-exec.h zesto-commit.h zesto-cache.h             \
zesto-dram.h zesto-bpred.h zesto-memdep.h zesto-prefetch.h zesto-uncore.h          \
zesto-MC.h zesto-dumps.h

ZOBJS = \
zesto-opts.$(OEXT) zesto-core.$(OEXT) zesto-oracle.$(OEXT) zesto-fetch.$(OEXT)     \
zesto-decode.$(OEXT) zesto-alloc.$(OEXT) zesto-exec.$(OEXT) zesto-commit.$(OEXT)   \
zesto-cache.$(OEXT) zesto-dram.$(OEXT) zesto-bpred.$(OEXT) zesto-memdep.$(OEXT)    \
zesto-prefetch.$(OEXT) zesto-uncore.$(OEXT) zesto-MC.$(OEXT) zesto-dumps.$(OEXT)

EXOOBJS = \
libexo/libexo.$(OEXT) libexo/exolex.$(OEXT)

#
# programs to build
#

include make.target

#
# all targets, NOTE: library ordering is important...
#
default: sim-zesto
all: $(PROGS)

syscall.$(OEXT): syscall.c syscall.h thread.h
	gcc $(CFLAGS) -c $*.c

make.target:
	touch make.target

sysprobe$(EEXT):	sysprobe.c
	$(CC) $(FFLAGS) -o sysprobe$(EEXT) sysprobe.c
	@echo endian probe results: $(ENDIAN)
	@echo probe flags: $(MFLAGS)
	@echo probe libs: $(MLIBS)
	-$(RM) libexo$(X)sysprobe$(EEXT)
	$(LN) ../sysprobe$(EEXT) libexo$(X)sysprobe$(EEXT)

sim-fast$(EEXT):	sysprobe$(EEXT) sim-fast.$(OEXT) $(OBJS) $(EXOOBJS) bbtracker.$(OEXT)
	$(CC) -o sim-fast$(EEXT) $(CFLAGS) sim-fast.$(OEXT) $(OBJS) bbtracker.$(OEXT) $(EXOOBJS) $(MLIBS)

sim-uop$(EEXT):	sysprobe$(EEXT) sim-uop.$(OEXT) $(OBJS) $(EXOOBJS)
	$(CC) -o sim-uop$(EEXT) $(CFLAGS) sim-uop.$(OEXT) $(OBJS) $(EXOOBJS) $(MLIBS)

sim-eio$(EEXT):	sysprobe$(EEXT) sim-eio.$(OEXT) $(OBJS) $(EXOOBJS)
	$(CC) -o sim-eio$(EEXT) $(CFLAGS) sim-eio.$(OEXT) $(OBJS) $(EXOOBJS) $(MLIBS)

sim-cache$(EEXT):	sysprobe$(EEXT) sim-cache.$(OEXT) $(OBJS) $(EXOOBJS) zesto-cache.$(OEXT) zesto-core.$(OEXT)
	$(CC) -o sim-cache$(EEXT) $(CFLAGS) sim-cache.$(OEXT) $(OBJS) zesto-cache.$(OEXT) zesto-core.$(OEXT) $(EXOOBJS) $(MLIBS)

sim-bpred$(EEXT):	sysprobe$(EEXT) sim-bpred.$(OEXT) $(OBJS) $(EXOOBJS) zesto-bpred.$(OEXT)
	$(CC) -o sim-bpred$(EEXT) $(CFLAGS) sim-bpred.$(OEXT) $(OBJS) $(EXOOBJS) zesto-bpred.$(OEXT) $(MLIBS)

sim-zesto$(EEXT):	sysprobe$(EEXT) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS)
	$(CC) -o sim-zesto$(EEXT) $(CFLAGS) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS) $(MLIBS)
sim-zesto2$(EEXT):	sysprobe$(EEXT) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS)
	$(CC) -o sim-zesto2$(EEXT) $(CFLAGS) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS) $(MLIBS)
sim-zesto3$(EEXT):	sysprobe$(EEXT) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS)
	$(CC) -o sim-zesto3$(EEXT) $(CFLAGS) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS) $(MLIBS)
sim-zesto4$(EEXT):	sysprobe$(EEXT) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS)
	$(CC) -o sim-zesto4$(EEXT) $(CFLAGS) sim-zesto.$(OEXT) $(OBJS) $(ZOBJS) $(EXOOBJS) $(MLIBS)
lib:	CFLAGS += -DZESTO_PIN	
lib:	sysprobe$(EEXT) sim-slave.$(OEXT) $(OBJS_SLAVE) $(ZOBJS) $(EXOOBJS)
	ar rs libsim.a sim-slave.$(OEXT) $(OBJS_SLAVE) $(ZOBJS) $(EXOOBJS)
	ranlib libsim.a
libd:	CFLAGS += -DZESTO_PIN -DZESTO_PIN_DBG -DZTRACE
libd:	sysprobe$(EEXT) sim-slave.$(OEXT) $(OBJS_SLAVE) $(ZOBJS) $(EXOOBJS)
	ar rs libsim.a sim-slave.$(OEXT) $(OBJS_SLAVE) $(ZOBJS) $(EXOOBJS)
	ranlib libsim.a

exo $(EXOOBJS): sysprobe$(EEXT)
	cd libexo $(CS) \
	$(MAKE) "MAKE=$(MAKE)" "CC=$(CC)" "AR=$(AR)" "AROPT=$(AROPT)" "RANLIB=$(RANLIB)" "CFLAGS=$(MFLAGS) $(FFLAGS) $(OFLAGS)" "OEXT=$(OEXT)" "LEXT=$(LEXT)" "EEXT=$(EEXT)" "X=$(X)" "RM=$(RM)" libexo.$(LEXT)


# The various *.list files are automatically generated from
# directory listings of the respective source directories.
zesto-fetch.$(OEXT): zesto-fetch.cpp zesto-fetch.h ZPIPE-fetch
	perl make_def_lists.pl fetch
	$(CC) $(CFLAGS) -c $*.cpp

zesto-decode.$(OEXT): zesto-decode.cpp zesto-decode.h ZPIPE-decode
	perl make_def_lists.pl decode
	$(CC) $(CFLAGS) -c $*.cpp

zesto-alloc.$(OEXT): zesto-alloc.cpp zesto-alloc.h ZPIPE-alloc
	perl make_def_lists.pl alloc
	$(CC) $(CFLAGS) -c $*.cpp

zesto-exec.$(OEXT): zesto-exec.cpp zesto-exec.h ZPIPE-exec
	perl make_def_lists.pl exec
	$(CC) $(CFLAGS) -c $*.cpp

zesto-commit.$(OEXT): zesto-commit.cpp zesto-commit.h ZPIPE-commit
	perl make_def_lists.pl commit
	$(CC) $(CFLAGS) -c $*.cpp

zesto-bpred.$(OEXT): zesto-bpred.cpp zesto-bpred.h ZCOMPS-bpred ZCOMPS-fusion ZCOMPS-btb ZCOMPS-ras
	perl make_def_lists.pl bpred
	$(CC) $(CFLAGS) -c $*.cpp

zesto-memdep.$(OEXT): zesto-memdep.cpp zesto-memdep.h ZCOMPS-memdep
	perl make_def_lists.pl memdep
	$(CC) $(CFLAGS) -c $*.cpp

zesto-prefetch.$(OEXT): zesto-prefetch.cpp zesto-prefetch.h ZCOMPS-prefetch
	perl make_def_lists.pl prefetch
	$(CC) $(CFLAGS) -c $*.cpp

zesto-dram.$(OEXT): zesto-dram.cpp zesto-dram.h ZCOMPS-dram
	perl make_def_lists.pl dram
	$(CC) $(CFLAGS) -c $*.cpp

zesto-MC.$(OEXT): zesto-MC.cpp zesto-MC.h ZCOMPS-MC
	perl make_def_lists.pl MC
	$(CC) $(CFLAGS) -c $*.cpp

.c.$(OEXT):
	$(CC) $(CFLAGS) -c $*.c

.cpp.$(OEXT):
	$(CC) $(CFLAGS) -c $*.cpp

filelist:
	@echo $(SRCS) $(HDRS) Makefile

clean:
	-$(RM) *.o *.obj core *~ Makefile.bak libsim* sysprobe$(EEXT) $(PROGS)
	cd libexo $(CS) $(MAKE) "RM=$(RM)" "CS=$(CS)" clean $(CS) cd ..

.PHONY: tags
tags: 
	ctags -R --extra=+q .

test: sim-zesto
	@ echo "### Testing simple single-core program ... " | chomp
	@ ./sim-zesto -config config/merom.cfg -config dram-config/DDR2-800-5-5-5.cfg tests/fib.eio.gz 2>&1 | \
    egrep -v "^sim: simulation started|sim_elapsed_time|sim_cycle_rate|all_inst_rate|sim_inst_rate|sim_uop_rate|sim_eff_uop_rate" \
    > tests/fib-test.out
	@ diff tests/fib-test.out tests/fib.out && echo "passed" || echo "failed!"
	@ $(RM) -f tests/fib-test.out
	@ echo "### Testing simple dual-core program ... " | chomp
	@ ./sim-zesto -config config/merom.cfg -config dram-config/DDR2-800-5-5-5.cfg -cores 2 \
    -max:inst 100000 -tracelimit 1000000 \tests/app1.eio.gz tests/app2.eio.gz 2>&1 | \
    egrep -v "^sim: simulation started|sim_elapsed_time|sim_cycle_rate|sim_inst_rate|sim_uop_rate|sim_eff_uop_rate|total_inst_rate|total_uop_rate|total_eff_uop_rate" \
    > tests/dual-core-test.out
	@ diff tests/dual-core-test.out tests/dual-core.out && echo "passed" || echo "failed!"
	@ $(RM) -f tests/dual-core-test.out

#

bbtracker.o: /usr/include/stdlib.h /usr/include/stdio.h /usr/include/malloc.h
bbtracker.o: bbtracker.h
eio.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
eio.o: memory.h stats.h eval.h loader.h thread.h libexo/libexo.h host.h
eio.o: misc.h syscall.h sim.h endian.h eio.h
endian.o: endian.h thread.h machine.h host.h misc.h machine.def
endian.o: zesto-structs.h regs.h options.h memory.h stats.h eval.h loader.h
eval.o: host.h misc.h eval.h machine.h machine.def zesto-structs.h regs.h
eval.o: options.h
loader.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
loader.o: options.h endian.h thread.h memory.h stats.h eval.h sim.h eio.h
loader.o: loader.h elf.h
machine.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
machine.o: options.h eval.h memory.h stats.h sim.h thread.h
machine.o: x86flow.def
main.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
main.o: endian.h thread.h memory.h stats.h eval.h version.h loader.h sim.h
main.o: interface.h
slave.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
slave.o: endian.h thread.h memory.h stats.h eval.h version.h loader.h sim.h
slave.o: interface.h callbacks.h
callbacks.o: callbacks.h interface.h
slices.o: stats.h host.h eval.h thread.h machine.h memory.h regs.h
slices.o: zesto-core.h zesto-structs.h
sim-main.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
sim-main.o: options.h memory.h stats.h eval.h loader.h thread.h syscall.h
sim-main.o: sim.h zesto-opts.h zesto-core.h zesto-oracle.h zesto-fetch.h
sim-main.o: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
sim-main.o: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
sim-main.o: zesto-MC.h interface.h
sim-slave.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
sim-slave.o: options.h memory.h stats.h eval.h loader.h thread.h syscall.h
sim-slave.o: sim.h zesto-opts.h zesto-core.h zesto-oracle.h zesto-fetch.h
sim-slave.o: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
sim-slave.o: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
sim-slave.o: zesto-MC.h interface.h callbacks.h
memory.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
memory.o: options.h stats.h eval.h memory.h interface.h callbacks.h
misc.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
options.o: host.h misc.h options.h
range.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
range.o: symbol.h loader.h memory.h stats.h eval.h thread.h range.h
regs.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
regs.o: loader.h memory.h stats.h eval.h thread.h
sim-eio.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
sim-eio.o: options.h memory.h stats.h eval.h loader.h thread.h syscall.h
sim-eio.o: eio.h range.h sim.h
sim-fast.o: host.h misc.h thread.h machine.h machine.def zesto-structs.h
sim-fast.o: regs.h options.h memory.h stats.h eval.h loader.h syscall.h sim.h
sim-fast.o: bbtracker.h
stats.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
stats.o: eval.h stats.h
symbol.o: host.h misc.h loader.h machine.h machine.def zesto-structs.h regs.h
symbol.o: options.h memory.h stats.h eval.h thread.h symbol.h elf.h
syscall.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
syscall.o: regs.h options.h memory.h stats.h eval.h loader.h sim.h endian.h
syscall.o: eio.h syscall.h
sysprobe.o: host.h misc.h endian.c endian.h thread.h machine.h machine.def
sysprobe.o: zesto-structs.h regs.h options.h memory.h stats.h eval.h loader.h
sim-cache.o: host.h misc.h thread.h machine.h machine.def zesto-structs.h
sim-cache.o: regs.h options.h memory.h stats.h eval.h loader.h syscall.h
sim-cache.o: sim.h zesto-core.h zesto-cache.h zesto-uncore.h zesto-MC.h
loader.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
loader.o: options.h endian.h thread.h memory.h stats.h eval.h sim.h eio.h
loader.o: loader.h elf.h
symbol.o: host.h misc.h loader.h machine.h machine.def zesto-structs.h regs.h
symbol.o: options.h memory.h stats.h eval.h thread.h symbol.h elf.h
syscall.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
syscall.o: regs.h options.h memory.h stats.h eval.h loader.h sim.h endian.h
syscall.o: eio.h syscall.h
x86.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
x86.o: eval.h memory.h stats.h sim.h thread.h x86flow.def
sim-zesto.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
sim-zesto.o: options.h memory.h stats.h eval.h loader.h thread.h syscall.h
sim-zesto.o: sim.h zesto-opts.h zesto-core.h zesto-oracle.h zesto-fetch.h
sim-zesto.o: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
sim-zesto.o: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
sim-zesto.o: zesto-MC.h
libsim.a: host.h misc.h machine.h machine.def zesto-structs.h regs.h
libsim.a: options.h memory.h stats.h eval.h loader.h thread.h syscall.h
libsim.a: sim.h zesto-opts.h zesto-core.h zesto-oracle.h zesto-fetch.h
libsim.a: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
libsim.a: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
libsim.a: zesto-MC.h interface.h callbacks.h
zesto-core.o: zesto-core.h zesto-structs.h machine.h host.h misc.h
zesto-core.o: machine.def regs.h options.h
zesto-opts.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-opts.o: regs.h options.h memory.h stats.h eval.h loader.h zesto-opts.h
zesto-opts.o: zesto-core.h zesto-oracle.h zesto-fetch.h zesto-decode.h
zesto-opts.o: zesto-alloc.h zesto-exec.h zesto-cache.h zesto-commit.h
zesto-opts.o: zesto-dram.h zesto-uncore.h zesto-MC.h
zesto-oracle.o: misc.h thread.h machine.h host.h machine.def zesto-structs.h
zesto-oracle.o: regs.h options.h memory.h stats.h eval.h syscall.h loader.h
zesto-oracle.o: zesto-core.h zesto-opts.h zesto-oracle.h zesto-fetch.h
zesto-oracle.o: zesto-bpred.h zesto-decode.h zesto-alloc.h zesto-exec.h
zesto-oracle.o: zesto-commit.h zesto-cache.h callbacks.h
zesto-fetch.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-fetch.o: regs.h options.h memory.h stats.h eval.h zesto-core.h
zesto-fetch.o: zesto-opts.h zesto-oracle.h zesto-fetch.h zesto-alloc.h
zesto-fetch.o: zesto-cache.h zesto-decode.h zesto-prefetch.h zesto-bpred.h
zesto-fetch.o: zesto-exec.h zesto-commit.h zesto-uncore.h zesto-MC.h
zesto-decode.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-decode.o: regs.h options.h memory.h stats.h eval.h zesto-core.h
zesto-decode.o: zesto-opts.h zesto-oracle.h zesto-decode.h zesto-fetch.h
zesto-decode.o: zesto-bpred.h
zesto-alloc.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-alloc.o: regs.h options.h memory.h stats.h eval.h zesto-core.h
zesto-alloc.o: zesto-opts.h zesto-oracle.h zesto-decode.h zesto-alloc.h
zesto-alloc.o: zesto-exec.h zesto-commit.h
zesto-exec.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-exec.o: regs.h options.h memory.h stats.h eval.h zesto-core.h
zesto-exec.o: zesto-opts.h zesto-oracle.h zesto-alloc.h zesto-exec.h
zesto-exec.o: zesto-memdep.h zesto-prefetch.h zesto-cache.h zesto-uncore.h
zesto-exec.o: zesto-MC.h
zesto-commit.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-commit.o: zesto-structs.h regs.h eval.h memory.h thread.h zesto-core.h
zesto-commit.o: zesto-opts.h zesto-oracle.h zesto-fetch.h zesto-decode.h
zesto-commit.o: zesto-alloc.h zesto-exec.h zesto-cache.h zesto-commit.h
zesto-commit.o: zesto-bpred.h zesto-dumps.h
zesto-cache.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-cache.o: regs.h options.h memory.h stats.h eval.h zesto-core.h
zesto-cache.o: zesto-opts.h zesto-cache.h zesto-prefetch.h zesto-dram.h
zesto-cache.o: zesto-uncore.h zesto-MC.h
zesto-dram.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-dram.o: regs.h options.h memory.h stats.h eval.h zesto-opts.h
zesto-dram.o: zesto-cache.h zesto-dram.h zesto-uncore.h zesto-MC.h
zesto-bpred.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-bpred.o: zesto-structs.h regs.h eval.h memory.h thread.h valcheck.h
zesto-bpred.o: zesto-core.h zesto-bpred.h
zesto-memdep.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-memdep.o: zesto-structs.h regs.h eval.h memory.h thread.h valcheck.h
zesto-memdep.o: zesto-opts.h zesto-core.h zesto-memdep.h
zesto-prefetch.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-prefetch.o: zesto-structs.h regs.h eval.h memory.h thread.h valcheck.h
zesto-prefetch.o: zesto-opts.h zesto-core.h zesto-bpred.h zesto-cache.h
zesto-prefetch.o: zesto-prefetch.h zesto-uncore.h zesto-MC.h
zesto-uncore.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-uncore.o: regs.h options.h memory.h stats.h eval.h zesto-core.h
zesto-uncore.o: zesto-opts.h zesto-cache.h zesto-prefetch.h zesto-uncore.h
zesto-uncore.o: zesto-MC.h zesto-dram.h
zesto-MC.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-MC.o: regs.h options.h memory.h stats.h eval.h zesto-opts.h
zesto-MC.o: zesto-cache.h zesto-uncore.h zesto-MC.h zesto-dram.h

# SimpleScalar(TM) Tool Suite
# Copyright (C) 1994-2002 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
# All Rights Reserved. 
# 
# THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
# YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
# 
# No portion of this work may be used by any commercial entity, or for any
# commercial purpose, without the prior, written permission of SimpleScalar,
# LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
# as described below.
# 
# 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
# or implied. The user of the program accepts full responsibility for the
# application of the program and the use of any results.
# 
# 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
# downloaded, compiled, executed, copied, and modified solely for nonprofit,
# educational, noncommercial research, and noncommercial scholarship
# purposes provided that this notice in its entirety accompanies all copies.
# Copies of the modified software can be delivered to persons who use it
# solely for nonprofit, educational, noncommercial research, and
# noncommercial scholarship purposes provided that this notice in its
# entirety accompanies all copies.
# 
# 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
# PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
# 
# 4. No nonprofit user may place any restrictions on the use of this software,
# including as modified by the user, by any other authorized user.
# 
# 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
# in compiled or executable form as set forth in Section 2, provided that
# either: (A) it is accompanied by the corresponding machine-readable source
# code, or (B) it is accompanied by a written offer, with no time limit, to
# give anyone a machine-readable copy of the corresponding source code in
# return for reimbursement of the cost of distribution. This written offer
# must permit verbatim duplication by anyone, or (C) it is distributed by
# someone who received only the executable form, and is accompanied by a
# copy of the written offer of source code.
# 
# 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
# currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
# 2395 Timbercrest Court, Ann Arbor, MI 48105.
# 
# Copyright (C) 2000-2002 by The Regents of The University of Michigan.
# Copyright (C) 1994-2002 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
#
#
# Copyright © 2009 by Gabriel H. Loh and the Georgia Tech Research Corporation
# Atlanta, GA  30332-0415
# All Rights Reserved.
# 
# THIS IS A LEGAL DOCUMENT BY DOWNLOADING ZESTO, YOU ARE AGREEING TO THESE
# TERMS AND CONDITIONS.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
# 
# NOTE: Portions of this release are directly derived from the SimpleScalar
# Toolset (property of SimpleScalar LLC), and as such, those portions are
# bound by the corresponding legal terms and conditions.  All source files
# derived directly or in part from the SimpleScalar Toolset bear the original
# user agreement.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 
# 3. Neither the name of the Georgia Tech Research Corporation nor the names of
# its contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# 4. Zesto is distributed freely for commercial and non-commercial use.  Note,
# however, that the portions derived from the SimpleScalar Toolset are bound
# by the terms and agreements set forth by SimpleScalar, LLC.  In particular:
# 
#   "Nonprofit and noncommercial use is encouraged. SimpleScalar may be
#   downloaded, compiled, executed, copied, and modified solely for nonprofit,
#   educational, noncommercial research, and noncommercial scholarship
#   purposes provided that this notice in its entirety accompanies all copies.
#   Copies of the modified software can be delivered to persons who use it
#   solely for nonprofit, educational, noncommercial research, and
#   noncommercial scholarship purposes provided that this notice in its
#   entirety accompanies all copies."
# 
# User is responsible for reading and adhering to the terms set forth by
# SimpleScalar, LLC where appropriate.
# 
# 5. No nonprofit user may place any restrictions on the use of this software,
# including as modified by the user, by any other authorized user.
# 
# 6. Noncommercial and nonprofit users may distribute copies of Zesto in
# compiled or executable form as set forth in Section 2, provided that either:
# (A) it is accompanied by the corresponding machine-readable source code, or
# (B) it is accompanied by a written offer, with no time limit, to give anyone
# a machine-readable copy of the corresponding source code in return for
# reimbursement of the cost of distribution. This written offer must permit
# verbatim duplication by anyone, or (C) it is distributed by someone who
# received only the executable form, and is accompanied by a copy of the
# written offer of source code.
# 
# 7. Zesto was developed by Gabriel H. Loh, Ph.D.  US Mail: 266 Ferst Drive,
# Georgia Institute of Technology, Atlanta, GA 30332-0765
