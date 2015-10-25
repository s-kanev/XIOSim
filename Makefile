# Makefile - simulator suite make file
#
# Please see copyright/licensing notices at the end of this file

##################################################################
#
# Modify the following definitions to suit your build environment,
# NOTE: most platforms should not require any changes
#
##################################################################

CXX?= g++

##################################################################

OFLAGS = -O3 -g -m32 -DUSE_SSE_MOVE -DDEBUG -msse4a -mfpmath=sse -std=c++11 -Werror -Wall -Wno-unused-function -Wno-strict-aliasing

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
PINTOOL_INC = -Ipintool
ifneq ($(CONFUSE_HOME),)
  CONFUSE_INC = -I$(CONFUSE_HOME)/include
else
  CONFUSE_INC =
endif
ifneq ($(BOOST_HOME),)
    BOOST_INC = -I$(BOOST_HOME)
else
    BOOST_INC =
endif


ifeq ($(wildcard $(PIN_ROOT)/source/tools/Config/unix.vars),)
    $(error Set PIN_ROOT to a valid Pin installation)
endif

XED_INC = -I$(PIN_ROOT)/extras/xed2-ia32/include

CFLAGS = $(FFLAGS) $(OFLAGS) $(ZTRACE) $(MCPAT_INC) $(CONFUSE_INC) $(REPEATER_INC) $(PINTOOL_INC) $(XED_INC) $(BOOST_INC)

#
# all the sources
#
SRCS =  \
memory.cpp         misc.cpp      expression.cpp\
stats.cpp         sim.cpp          slices.cpp\
decode.cpp        uop_cracker.cpp  fu.cpp\
sim-loop.cpp

HDRS = \
libsim.h        host.h          memory.h\
misc.h                   sim.h           stats.h\
pintool/buffer.h\
decode.h        uop_cracker.h   fu.h\
sim-loop.h      expression.h    stat_database.h

OBJS =	\
memory.$(OEXT)       misc.$(OEXT)\
stats.$(OEXT)        slices.$(OEXT)     sim.$(OEXT)\
decode.$(OEXT)       uop_cracker.$(OEXT)  fu.$(OEXT)\
sim-loop.$(OEXT)     expression.$(OEXT)

# Zesto specific files
ZSRCS = \
zesto-core.cpp zesto-oracle.cpp zesto-fetch.cpp         \
zesto-decode.cpp zesto-alloc.cpp zesto-exec.cpp zesto-commit.cpp zesto-cache.cpp   \
zesto-dram.cpp zesto-bpred.cpp zesto-memdep.cpp zesto-prefetch.cpp                 \
zesto-uncore.cpp zesto-MC.cpp zesto-power.cpp zesto-noc.cpp        \
zesto-repeater.cpp zesto-coherence.cpp zesto-dvfs.cpp zesto-config.cpp \
zesto-config-params.cpp ztrace.cpp shadow_MopQ.cpp

ZHDRS = \
zesto-structs.h zesto-core.h zesto-oracle.h zesto-fetch.h             \
zesto-decode.h zesto-alloc.h zesto-exec.h zesto-commit.h zesto-cache.h             \
zesto-dram.h zesto-bpred.h zesto-memdep.h zesto-prefetch.h zesto-uncore.h          \
zesto-MC.h zesto-power.h zesto-coherence.h zesto-noc.h               \
zesto-repeater.h zesto-dvfs.h zesto-config.h ztrace.h shadow_MopQ.h

ZOBJS=$(ZSRCS:.cpp=.o)

#
# all targets
#
default: lib

lib:	$(OBJS) $(ZOBJS)
	ar rs libsim.a $(OBJS) $(ZOBJS)
	ranlib libsim.a
libd: CFLAGS += -DZTRACE -DZESTO_PIN_DBG
libd:	$(OBJS) $(ZOBJS)
	ar rs libsim.a $(OBJS) $(ZOBJS)
	ranlib libsim.a

# The various *.list files are automatically generated from
# directory listings of the respective source directories.
zesto-fetch.$(OEXT): zesto-fetch.cpp zesto-fetch.h ZPIPE-fetch
	perl make_def_lists.pl fetch
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-decode.$(OEXT): zesto-decode.cpp zesto-decode.h ZPIPE-decode
	perl make_def_lists.pl decode
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-alloc.$(OEXT): zesto-alloc.cpp zesto-alloc.h ZPIPE-alloc
	perl make_def_lists.pl alloc
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-exec.$(OEXT): zesto-exec.cpp zesto-exec.h ZPIPE-exec
	perl make_def_lists.pl exec
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-commit.$(OEXT): zesto-commit.cpp zesto-commit.h ZPIPE-commit
	perl make_def_lists.pl commit
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-bpred.$(OEXT): zesto-bpred.cpp zesto-bpred.h ZCOMPS-bpred ZCOMPS-fusion ZCOMPS-btb ZCOMPS-ras
	perl make_def_lists.pl bpred
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-memdep.$(OEXT): zesto-memdep.cpp zesto-memdep.h ZCOMPS-memdep
	perl make_def_lists.pl memdep
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-prefetch.$(OEXT): zesto-prefetch.cpp zesto-prefetch.h ZCOMPS-prefetch
	perl make_def_lists.pl prefetch
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-dram.$(OEXT): zesto-dram.cpp zesto-dram.h ZCOMPS-dram
	perl make_def_lists.pl dram
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-MC.$(OEXT): zesto-MC.cpp zesto-MC.h ZCOMPS-MC
	perl make_def_lists.pl MC
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-power.$(OEXT): zesto-power.cpp zesto-power.h ZCORE-power
	perl make_def_lists.pl power
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-coherence.$(OEXT): zesto-coherence.cpp zesto-coherence.h ZCOMPS-coherence
	perl make_def_lists.pl coherence
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-repeater.$(OEXT): zesto-repeater.cpp zesto-repeater.h ZCOMPS-repeater
	perl make_def_lists.pl repeater
	$(CXX) $(CFLAGS) -c $*.cpp

zesto-dvfs.$(OEXT): zesto-dvfs.cpp zesto-dvfs.h ZCOMPS-dvfs
	perl make_def_lists.pl dvfs
	$(CXX) $(CFLAGS) -c $*.cpp

.c.$(OEXT):
	$(CXX) $(CFLAGS) -c $*.c

.cpp.$(OEXT):
	$(CXX) $(CFLAGS) -c $*.cpp

clean:
	-$(RM) *.a *.o *.obj core

.PHONY: tags
tags:
	ctags -R --extra=+q .

eval.o: host.h misc.h  zesto-structs.h
sim.o: host.h misc.h zesto-structs.h
sim.o: memory.h stats.h sim.h
sim.o: libsim.h
slices.o: stats.h host.h  memory.h
slices.o: zesto-core.h zesto-structs.h
sim-loop.o: host.h misc.h zesto-structs.h
sim-loop.o:  memory.h stats.h libsim.h
sim-loop.o: sim.h zesto-core.h zesto-oracle.h zesto-fetch.h
sim-loop.o: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
sim-loop.o: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
sim-loop.o: zesto-MC.h synchronization.h
sim-loop.o: zesto-repeater.h
memory.o: host.h misc.h zesto-structs.h
memory.o:  stats.h  memory.h
misc.o: host.h misc.h zesto-structs.h
misc.o: synchronization.h
stats.o: host.h misc.h zesto-structs.h
stats.o:  stats.h
libsim.a: host.h misc.h zesto-structs.h
libsim.a:  memory.h stats.h
libsim.a: sim.h zesto-core.h zesto-oracle.h zesto-fetch.h
libsim.a: zesto-decode.h zesto-bpred.h zesto-alloc.h zesto-exec.h
libsim.a: zesto-commit.h zesto-dram.h zesto-cache.h zesto-uncore.h
libsim.a: zesto-MC.h synchronization.h
libsim.a: zesto-repeater.h
zesto-core.o: zesto-core.h zesto-structs.h host.h misc.h
zesto-core.o:
zesto-oracle.o: misc.h host.h zesto-structs.h
zesto-oracle.o:  memory.h stats.h
zesto-oracle.o: zesto-core.h zesto-oracle.h zesto-fetch.h
zesto-oracle.o: zesto-bpred.h zesto-decode.h zesto-alloc.h zesto-exec.h
zesto-oracle.o: zesto-commit.h zesto-cache.h
zesto-fetch.o: host.h misc.h zesto-structs.h
zesto-fetch.o:  memory.h stats.h  zesto-core.h
zesto-fetch.o: zesto-oracle.h zesto-fetch.h zesto-alloc.h
zesto-fetch.o: zesto-cache.h zesto-decode.h zesto-prefetch.h zesto-bpred.h
zesto-fetch.o: zesto-exec.h zesto-commit.h zesto-uncore.h zesto-MC.h
zesto-fetch.o: zesto-coherence.h zesto-noc.h
zesto-decode.o: host.h misc.h zesto-structs.h
zesto-decode.o:  memory.h stats.h  zesto-core.h
zesto-decode.o: zesto-oracle.h zesto-decode.h zesto-fetch.h
zesto-decode.o: zesto-bpred.h
zesto-alloc.o: host.h misc.h zesto-structs.h
zesto-alloc.o:  memory.h stats.h  zesto-core.h
zesto-alloc.o: zesto-oracle.h zesto-decode.h zesto-alloc.h
zesto-alloc.o: zesto-exec.h zesto-commit.h regs.h
zesto-exec.o: host.h misc.h zesto-structs.h regs.h
zesto-exec.o:  memory.h stats.h  zesto-core.h
zesto-exec.o: zesto-oracle.h zesto-alloc.h zesto-exec.h
zesto-exec.o: zesto-memdep.h zesto-prefetch.h zesto-cache.h zesto-uncore.h
zesto-exec.o: zesto-MC.h zesto-repeater.h zesto-coherence.h zesto-noc.h
zesto-commit.o: sim.h  stats.h host.h misc.h regs.h
zesto-commit.o: zesto-structs.h  memory.h zesto-core.h
zesto-commit.o: zesto-oracle.h zesto-fetch.h zesto-decode.h
zesto-commit.o: zesto-alloc.h zesto-exec.h zesto-cache.h zesto-commit.h
zesto-commit.o: zesto-bpred.h zesto-repeater.h
zesto-power.o: sim.h  stats.h host.h misc.h
zesto-power.o: zesto-structs.h  memory.h zesto-core.h
zesto-power.o: zesto-oracle.h zesto-fetch.h zesto-decode.h
zesto-power.o: zesto-alloc.h zesto-exec.h zesto-cache.h zesto-commit.h
zesto-power.o: zesto-bpred.h zesto-uncore.h mcpat/mcpat.h
zesto-power.o: mcpat/XML_Parse.h
zesto-cache.o: host.h misc.h zesto-structs.h
zesto-cache.o:  memory.h stats.h  zesto-core.h
zesto-cache.o: zesto-cache.h zesto-prefetch.h zesto-dram.h
zesto-cache.o: zesto-uncore.h zesto-MC.h zesto-coherence.h zesto-noc.h
zesto-coherence.o: host.h misc.h zesto-structs.h
zesto-coherence.o:  memory.h stats.h  zesto-core.h
zesto-coherence.o: zesto-cache.h zesto-prefetch.h zesto-dram.h
zesto-coherence.o: zesto-uncore.h zesto-MC.h zesto-coherence.h zesto-noc.h
zesto-noc.o: host.h misc.h zesto-structs.h
zesto-noc.o:  memory.h stats.h  zesto-core.h
zesto-noc.o: zesto-cache.h zesto-prefetch.h zesto-dram.h
zesto-noc.o: zesto-uncore.h zesto-MC.h zesto-coherence.h zesto-noc.h
zesto-dram.o: host.h misc.h zesto-structs.h
zesto-dram.o:  memory.h stats.h
zesto-dram.o: zesto-cache.h zesto-dram.h zesto-uncore.h zesto-MC.h
zesto-bpred.o: sim.h  stats.h host.h misc.h
zesto-bpred.o: zesto-structs.h  memory.h valcheck.h
zesto-bpred.o: zesto-core.h zesto-bpred.h
zesto-memdep.o: sim.h  stats.h host.h misc.h
zesto-memdep.o: zesto-structs.h  memory.h valcheck.h
zesto-memdep.o: zesto-core.h zesto-memdep.h
zesto-prefetch.o: sim.h  stats.h host.h misc.h
zesto-prefetch.o: zesto-structs.h  memory.h valcheck.h
zesto-prefetch.o: zesto-core.h zesto-bpred.h zesto-cache.h
zesto-prefetch.o: zesto-prefetch.h zesto-uncore.h zesto-MC.h
zesto-uncore.o: host.h misc.h zesto-structs.h
zesto-uncore.o: memory.h stats.h  zesto-core.h
zesto-uncore.o: zesto-cache.h zesto-prefetch.h zesto-uncore.h
zesto-uncore.o: zesto-MC.h zesto-dram.h zesto-noc.h zesto-coherence.h
zesto-MC.o: host.h misc.h zesto-structs.h
zesto-MC.o: memory.h stats.h
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
