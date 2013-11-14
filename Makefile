# Makefile - simulator suite make file
#
# Please see copyright/licensing notices at the end of this file

##################################################################
#
# Modify the following definitions to suit your build environment,
# NOTE: most platforms should not require any changes
#
##################################################################

CC ?= g++

##################################################################
# Uncomment only one of the following OFLAGS, or make your own

# For debug:
OFLAGS = -O3 -g -m32 -DMIN_SYSCALL_MODE -DUSE_SSE_MOVE -DDEBUG -Wall -msse4a -mfpmath=sse -std=c++11

# Fully-optimized, but with profiling for gprof:
#OFLAGS = -O3 -g -pg -m32 -DMIN_SYSCALL_MODE -DUSE_SSE_MOVE -Wall -static -fexpensive-optimizations -mtune=core2 -march=core2 -msse4a -mfpmath=sse -funroll-loops

# Fully-optimized:
#OFLAGS = -O3 -m32 -g -DNDEBUG -DMIN_SYSCALL_MODE -DUSE_SSE_MOVE -Wall -static  -msse4a -mfpmath=sse

##################################################################
# Uncomment to turn on pipeline event logging 
ZTRACE = #-DZTRACE

##################################################################
MAKE = make
AR = ar qcv
RM = rm -f
OEXT = o

# Compilation-specific feature flags
#
# -DDEBUG	- turns on debugging features
# -DGZIP_PATH	- specifies path to GZIP executable
##################################################################
#
# complete flags
#
MCPAT_INC = -Imcpat
REPEATER_INC = -Imem-repeater

CFLAGS = $(FFLAGS) $(OFLAGS) $(BINUTILS_INC) $(BINUTILS_LIB) $(ZTRACE) $(MCPAT_INC) $(REPEATER_INC) -DZESTO_PIN

#
# all the sources
#
SRCS =  \
eval.c          machine.c       memory.c         misc.c         options.c   \
stats.c         slave.c         sim-main.c       callbacks.c    slices.cpp  \
buffer.cpp

HDRS = \
thread.h                  host.h          machine.h       memory.h           \
misc.h          options.h       regs.h          sim.h           stats.h         version.h          \
machine.def     x86flow.def     interface.h     callbacks.h     buffer.h

OBJS =	\
eval.$(OEXT)         machine.$(OEXT)      memory.$(OEXT)       misc.$(OEXT)          options.$(OEXT)    \
stats.$(OEXT)        sim-main.$(OEXT)     slices.$(OEXT)       callbacks.$(OEXT)     slave.$(OEXT)      \
buffer.$(OEXT)

# Zesto specific files
ZSRCS = \
zesto-core.cpp zesto-opts.cpp zesto-oracle.cpp zesto-fetch.cpp         \
zesto-decode.cpp zesto-alloc.cpp zesto-exec.cpp zesto-commit.cpp zesto-cache.cpp   \
zesto-dram.cpp zesto-bpred.cpp zesto-memdep.cpp zesto-prefetch.cpp                 \
zesto-uncore.cpp zesto-MC.cpp zesto-dumps.cpp zesto-power.cpp zesto-noc.cpp        \
zesto-repeater.cpp zesto-coherence.cpp zesto-dvfs.cpp

ZHDRS = \
zesto-structs.h zesto-core.h zesto-opts.h zesto-oracle.h zesto-fetch.h             \
zesto-decode.h zesto-alloc.h zesto-exec.h zesto-commit.h zesto-cache.h             \
zesto-dram.h zesto-bpred.h zesto-memdep.h zesto-prefetch.h zesto-uncore.h          \
zesto-MC.h zesto-dumps.h zesto-power.h zesto-coherence.h zesto-noc.h               \
zesto-repeater.h zesto-dvfs.h

ZOBJS=$(ZSRCS:.cpp=.o)

#
# all targets
#
default: lib

lib:	sim-slave.$(OEXT) $(OBJS) $(ZOBJS)
	ar rs libsim.a sim-slave.$(OEXT) $(OBJS) $(ZOBJS)
	ranlib libsim.a
libd: CFLAGS += -DZTRACE -DZESTO_PIN_DBG
libd:	sim-slave.$(OEXT) $(OBJS) $(ZOBJS)
	ar rs libsim.a sim-slave.$(OEXT) $(OBJS) $(ZOBJS)
	ranlib libsim.a

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

zesto-power.$(OEXT): zesto-power.cpp zesto-power.h ZCORE-power
	perl make_def_lists.pl power
	$(CC) $(CFLAGS) -c $*.cpp

zesto-coherence.$(OEXT): zesto-coherence.cpp zesto-coherence.h ZCOMPS-coherence
	perl make_def_lists.pl coherence
	$(CC) $(CFLAGS) -c $*.cpp

zesto-repeater.$(OEXT): zesto-repeater.cpp zesto-repeater.h ZCOMPS-repeater
	perl make_def_lists.pl repeater
	$(CC) $(CFLAGS) -c $*.cpp

zesto-dvfs.$(OEXT): zesto-dvfs.cpp zesto-dvfs.h ZCOMPS-dvfs
	perl make_def_lists.pl dvfs
	$(CC) $(CFLAGS) -c $*.cpp

.c.$(OEXT):
	$(CC) $(CFLAGS) -c $*.c

.cpp.$(OEXT):
	$(CC) $(CFLAGS) -c $*.cpp

clean:
	-$(RM) *.o *.obj core libsim*

.PHONY: tags
tags: 
	ctags -R --extra=+q .

eval.o: host.h misc.h  machine.h machine.def zesto-structs.h regs.h
eval.o: options.h
machine.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
machine.o: options.h  memory.h stats.h sim.h thread.h
machine.o: x86flow.def
slave.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
slave.o:  thread.h memory.h stats.h  version.h sim.h
slave.o: interface.h callbacks.h
callbacks.o: callbacks.h interface.h
slices.o: stats.h host.h  thread.h machine.h memory.h regs.h
slices.o: zesto-core.h zesto-structs.h
sim-main.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
sim-main.o: options.h memory.h stats.h thread.h
sim-main.o: sim.h zesto-opts.h zesto-core.h zesto-oracle.h zesto-fetch.h
sim-main.o: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
sim-main.o: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
sim-main.o: zesto-MC.h interface.h
sim-slave.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
sim-slave.o: options.h memory.h stats.h thread.h
sim-slave.o: sim.h zesto-opts.h zesto-core.h zesto-oracle.h zesto-fetch.h
sim-slave.o: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
sim-slave.o: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
sim-slave.o: zesto-MC.h interface.h callbacks.h synchronization.h
sim-slave.o: zesto-repeater.h
memory.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h
memory.o: options.h stats.h  memory.h interface.h callbacks.h
misc.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
misc.o: synchronization.h
options.o: host.h misc.h options.h
stats.o: host.h misc.h machine.h machine.def zesto-structs.h regs.h options.h
stats.o:  stats.h
libsim.a: host.h misc.h machine.h machine.def zesto-structs.h regs.h
libsim.a: options.h memory.h stats.h thread.h
libsim.a: sim.h zesto-opts.h zesto-core.h zesto-oracle.h zesto-fetch.h
libsim.a: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
libsim.a: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
libsim.a: zesto-MC.h interface.h callbacks.h synchronization.h
libsim.a: zesto-repeater.h
zesto-core.o: zesto-core.h zesto-structs.h machine.h host.h misc.h
zesto-core.o: machine.def regs.h options.h
zesto-opts.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-opts.o: regs.h options.h memory.h stats.h zesto-opts.h
zesto-opts.o: zesto-core.h zesto-oracle.h zesto-fetch.h zesto-decode.h
zesto-opts.o: zesto-alloc.h zesto-exec.h zesto-cache.h zesto-commit.h
zesto-opts.o: zesto-dram.h zesto-uncore.h zesto-MC.h zesto-repeater.h
zesto-oracle.o: misc.h thread.h machine.h host.h machine.def zesto-structs.h
zesto-oracle.o: regs.h options.h memory.h stats.h
zesto-oracle.o: zesto-core.h zesto-opts.h zesto-oracle.h zesto-fetch.h
zesto-oracle.o: zesto-bpred.h zesto-decode.h zesto-alloc.h zesto-exec.h
zesto-oracle.o: zesto-commit.h zesto-cache.h callbacks.h
zesto-fetch.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-fetch.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-fetch.o: zesto-opts.h zesto-oracle.h zesto-fetch.h zesto-alloc.h
zesto-fetch.o: zesto-cache.h zesto-decode.h zesto-prefetch.h zesto-bpred.h
zesto-fetch.o: zesto-exec.h zesto-commit.h zesto-uncore.h zesto-MC.h
zesto-fetch.o: zesto-coherence.h zesto-noc.h
zesto-decode.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-decode.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-decode.o: zesto-opts.h zesto-oracle.h zesto-decode.h zesto-fetch.h
zesto-decode.o: zesto-bpred.h
zesto-alloc.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-alloc.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-alloc.o: zesto-opts.h zesto-oracle.h zesto-decode.h zesto-alloc.h
zesto-alloc.o: zesto-exec.h zesto-commit.h
zesto-exec.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-exec.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-exec.o: zesto-opts.h zesto-oracle.h zesto-alloc.h zesto-exec.h
zesto-exec.o: zesto-memdep.h zesto-prefetch.h zesto-cache.h zesto-uncore.h
zesto-exec.o: zesto-MC.h zesto-repeater.h zesto-coherence.h zesto-noc.h
zesto-commit.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-commit.o: zesto-structs.h regs.h  memory.h thread.h zesto-core.h
zesto-commit.o: zesto-opts.h zesto-oracle.h zesto-fetch.h zesto-decode.h
zesto-commit.o: zesto-alloc.h zesto-exec.h zesto-cache.h zesto-commit.h
zesto-commit.o: zesto-bpred.h zesto-dumps.h zesto-repeater.h
zesto-power.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-power.o: zesto-structs.h regs.h  memory.h thread.h zesto-core.h
zesto-power.o: zesto-opts.h zesto-oracle.h zesto-fetch.h zesto-decode.h
zesto-power.o: zesto-alloc.h zesto-exec.h zesto-cache.h zesto-commit.h
zesto-power.o: zesto-bpred.h zesto-dumps.h zesto-uncore.h mcpat/mcpat.h
zesto-power.o: mcpat/XML_Parse.h
zesto-cache.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-cache.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-cache.o: zesto-opts.h zesto-cache.h zesto-prefetch.h zesto-dram.h
zesto-cache.o: zesto-uncore.h zesto-MC.h zesto-coherence.h zesto-noc.h
zesto-coherence.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-coherence.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-coherence.o: zesto-opts.h zesto-cache.h zesto-prefetch.h zesto-dram.h
zesto-coherence.o: zesto-uncore.h zesto-MC.h zesto-coherence.h zesto-noc.h
zesto-noc.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-noc.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-noc.o: zesto-opts.h zesto-cache.h zesto-prefetch.h zesto-dram.h
zesto-noc.o: zesto-uncore.h zesto-MC.h zesto-coherence.h zesto-noc.h
zesto-dram.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-dram.o: regs.h options.h memory.h stats.h  zesto-opts.h
zesto-dram.o: zesto-cache.h zesto-dram.h zesto-uncore.h zesto-MC.h
zesto-bpred.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-bpred.o: zesto-structs.h regs.h  memory.h thread.h valcheck.h
zesto-bpred.o: zesto-core.h zesto-bpred.h
zesto-memdep.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-memdep.o: zesto-structs.h regs.h  memory.h thread.h valcheck.h
zesto-memdep.o: zesto-opts.h zesto-core.h zesto-memdep.h
zesto-prefetch.o: sim.h options.h stats.h host.h machine.h misc.h machine.def
zesto-prefetch.o: zesto-structs.h regs.h  memory.h thread.h valcheck.h
zesto-prefetch.o: zesto-opts.h zesto-core.h zesto-bpred.h zesto-cache.h
zesto-prefetch.o: zesto-prefetch.h zesto-uncore.h zesto-MC.h
zesto-uncore.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-uncore.o: regs.h options.h memory.h stats.h  zesto-core.h
zesto-uncore.o: zesto-opts.h zesto-cache.h zesto-prefetch.h zesto-uncore.h
zesto-uncore.o: zesto-MC.h zesto-dram.h zesto-noc.h zesto-coherence.h
zesto-MC.o: thread.h machine.h host.h misc.h machine.def zesto-structs.h
zesto-MC.o: regs.h options.h memory.h stats.h  zesto-opts.h
zesto-MC.o: zesto-cache.h zesto-uncore.h zesto-MC.h zesto-dram.h
zesto-MC.o: zesto-coherence.h zesto-noc.h

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
