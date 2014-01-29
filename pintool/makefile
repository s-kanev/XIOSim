##
## This is a sample makefile for building Pin tools outside
## of the Pin environment.  This makefile is suitable for
## building with the Pin kit, not a Pin source development tree.
##
## To build the tool, execute the make command:
##
##      make
## or
##      make PIN_HOME=<top-level directory where Pin was installed>
##
## After building your tool, you would invoke Pin like this:
##
##      $PIN_HOME/pin -t MyPinTool -- /bin/ls
##
##############################################################
#
# User-specific configuration
#
##############################################################

#
# 1. Change PIN_HOME to point to the top-level directory where
#    Pin was installed. This can also be set on the command line,
#    or as an environment variable.
#
PIN_HOME ?= /home/skanev/pin/2.12
ZESTO_HOME = ..
BOOST_HOME ?= /home/samxi/boost_1_55_0
CONFUSE_HOME ?= /home/samxi/local/
TARGET = ia32

##############################################################
#
# set up and include *.config files
#
##############################################################

PIN_KIT=$(PIN_HOME)
KIT=1

TARGET_COMPILER?=gnu
ifeq ($(TARGET_COMPILER),gnu)
    include $(PIN_HOME)/source/tools/makefile.gnu.config
    CXXFLAGS ?= -I$(PIN_HOME)/source/tools/InstLib -Wall -Werror -Wno-unused-function -Wno-unknown-pragmas -g -std=c++11 $(DBG) $(OPT)
    PIN=$(PIN_HOME)/pin
endif

SIM_LIB = $(ZESTO_HOME)/libsim.a
MCPAT_LIB = $(ZESTO_HOME)/mcpat/mcpat.a

ifeq ($(wildcard $(ZESTO_HOME)/mem-repeater/mem-repeater.h),)
    REPEATER_LIB =
else
    REPEATER_LIB = $(ZESTO_HOME)/mem-repeater/librepeater.a
endif

ifeq ($(wildcard $(ZESTO_HOME)/DRAMSim2/DRAMSim.h),)
    DRAMSIM_LIB =
else
    DRAMSIM_LIB = $(ZESTO_HOME)/DRAMSim2/libdramsim.a
endif

BOOST_INCLUDE = $(BOOST_HOME)
BOOST_LDFLAGS = -lrt -lpthread
LIBCONFUSE_FLAGS = -I$(CONFUSE_HOME)include -L$(CONFUSE_HOME)lib

##############################################################
#
# Tools - you may wish to add your tool name to TOOL_ROOTS
#
##############################################################


TOOL_ROOTS = timing_sim

TOOL_OBJS = $(OBJDIR)timing_sim.o $(OBJDIR)lib.o $(OBJDIR)scheduler.o $(OBJDIR)multiprocess_shared.o  $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)BufferManagerConsumer.o $(OBJDIR)BufferManager.o

HARNESS_OBJS = $(OBJDIR)harness.o $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o

FEEDER_OBJS = $(OBJDIR)feeder_zesto.o $(ZESTO_HOME)/buffer.o $(OBJDIR)ildjit.o $(OBJDIR)fluffy.o $(OBJDIR)utils.o $(OBJDIR)sync_pthreads.o $(OBJDIR)parsec.o $(OBJDIR)syscall_handling.o $(OBJDIR)ignore_ins.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)ipc_queues.o $(OBJDIR)multiprocess_shared.o $(OBJDIR)BufferManagerProducer.o $(OBJDIR)BufferManager.o 

TOOLS = $(OBJDIR)$(TOOL_ROOTS)

STATIC_LIBS = mpkeys
SHM_LDFLAGS = -ldl

##############################################################
#
# build rules
#
##############################################################
all : CXXFLAGS += -DDEBUG -D_FILE_OFFSET_BITS=64
all : CXXFLAGS += -I$(BOOST_INCLUDE) $(BOOST_LDFLAGS) $(SHM_LDFLAGS)
all: tools $(OBJDIR)harness $(OBJDIR)feeder_zesto.so
debug:	CXXFLAGS += -DZESTO_PIN_DBG -D_FILE_OFFSET_BITS=64
debug: CXXFLAGS += -I$(BOOST_INCLUDE) $(BOOST_LDFLAGS) $(SHM_LDFLAGS)
debug: tools $(OBJDIR)harness $(OBJDIR)feeder_zesto.so
tools: $(OBJDIR) $(TOOLS)

##############################################################
# Subcomponents
$(ZESTO_HOME)/libsim.a:
	$(MAKE) -C $(ZESTO_HOME) lib

$(ZESTO_HOME)/DRAMSim2/libdramsim.a:
	$(MAKE) -C $(ZESTO_HOME)/DRAMSim2

$(ZESTO_HOME)/mcpat/mcpat.a:
	$(MAKE) -C $(ZESTO_HOME)/mcpat lib

$(ZESTO_HOME)/mem-repeater/librepeater.a:
	$(MAKE) -C $(ZESTO_HOME)/mem-repeater

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o : %.cpp
	$(CXX) -c -I$(BOOST_INCLUDE) $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(TOOL_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB)

$(TOOLS):
	${PIN_LD} -g -I$(ZESTO_HOME) -L$(ZESTO_HOME) -I$(BOOST_INCLUDE) $(LINK_DEBUG) ${LINK_OUT} $@ $^ $(DBG) -lrt -pthread 

$(OBJDIR)feeder_zesto.so: $(FEEDER_OBJS) 
	${PIN_LD} -g $(PIN_LDFLAGS) -I$(ZESTO_HOME) -I$(BOOST_INCLUDE) $(LINK_DEBUG) ${LINK_OUT} $@ ${PIN_LPATHS} $^ $(PIN_LIBS) $(DBG) -lrt -pthread 

$(OBJDIR)harness: $(HARNESS_OBJS)
	${PIN_LD} -g $(CXXFLAGS) ${LIBCONFUSE_FLAGS} ${SHM_LDFLAGS} $(LINK_DEBUG) ${LINK_OUT} $@ $^ -pthread -lrt -lconfuse

## cleaning
clean:
	-rm -rf $(OBJDIR) *.out *.tested *.failed makefile.copy
	$(MAKE) -C $(ZESTO_HOME) clean
	$(MAKE) -C $(ZESTO_HOME)/mcpat clean
	if [ -n "$(DRAMSIM_LIB)" ]; then $(MAKE) -C $(ZESTO_HOME)/DRAMSim2 clean; fi
	if [ -n "$(REPEATER_LIB)" ]; then $(MAKE) -C $(ZESTO_HOME)/mem-repeater clean; fi

.PHONY: clean-tool
clean-tool:
	-rm -rf $(OBJDIR) *.out *.tested *.failed makefile.copy


.PHONY: clean-harness
clean-harness:
	-rm -rf $(OBJDIR)harness *.failed makefile.copy

install:
	cp $(TOOLS) $(COOL_INSTALL_DIR)
	cp $(OBJDIR)feeder_zesto.so $(COOL_INSTALL_DIR)
	cp $(OBJDIR)harness $(COOL_INSTALL_DIR)
	git log -1 --format="%H" > $(COOL_INSTALL_DIR)/version
	git diff --no-prefix > $(COOL_INSTALL_DIR)/diff
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git log -1 --format="%H" > $(COOL_INSTALL_DIR)/repeater_version; fi
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git diff --no-prefix > $(COOL_INSTALL_DIR)/repeater_diff; fi
