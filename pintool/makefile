##
## This is a sample makefile for building Pin tools outside
## of the Pin environment.  This makefile is suitable for
## building with the Pin kit, not a Pin source development tree.
##
## To build the tool, execute the make command:
##
##      make
## or
##      make PIN_ROOT=<top-level directory where Pin was installed>
##
## After building your tool, you would invoke Pin like this:
##
##      $PIN_ROOT/pin -t MyPinTool -- /bin/ls
##
##############################################################
#
# User-specific configuration
#
##############################################################

#
# 1. Change PIN_ROOT to point to the top-level directory where
#    Pin was installed. This can also be set on the command line,
#    or as an environment variable.
#
PIN_ROOT ?= /home/skanev/pin/2.14
ZESTO_HOME = ..
TARGET = ia32

##############################################################
#
# set up and include *.config files
#
##############################################################

KIT = 1
COMPILER ?= gcc
TARGET_OS = linux

include $(PIN_ROOT)/source/tools/Config/unix.vars
include $(PIN_ROOT)/source/tools/Config/makefile.unix.config

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

# Since pin 2.13(4?) the version of InstLib having PinPoints suppot is no more,
# in favor of the shiny new PinPlay kit. Integrating with PinPlay is not surprisingly
# non-trivial for XIOSim, so for now we keep a local version of 'good ole InstLib'.
INSTLIB_HOME = InstLib
CXXFLAGS ?= -I$(INSTLIB_HOME)
##############################################################
#
# Tools - you may wish to add your tool name to TOOL_ROOTS
#
##############################################################


TOOL_ROOTS = feeder_zesto

TOOL_OBJS = $(OBJDIR)BufferManager.o $(OBJDIR)ildjit.o $(OBJDIR)fluffy.o $(OBJDIR)lib.o $(OBJDIR)utils.o $(OBJDIR)sync_pthreads.o $(OBJDIR)scheduler.o $(OBJDIR)parsec.o $(OBJDIR)machsuite.o $(OBJDIR)syscall_handling.o $(OBJDIR)ignore_ins.o

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))


##############################################################
#
# build rules
#
##############################################################
all : CXXFLAGS += -DDEBUG -D_FILE_OFFSET_BITS=64 -std=c++11
all: tools
debug:	CXXFLAGS += -DZESTO_PIN_DBG -D_FILE_OFFSET_BITS=64 -std=c++11
debug:	tools
tools: $(OBJDIR) $(TOOLS)
test: $(OBJDIR) $(TOOL_ROOTS:%=%.test)

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

$(OBJDIR)%.o : %.C
	$(CXX) -c $(CXXFLAGS) $(TOOL_CXXFLAGS) ${COMP_OBJ}$@ $<

$(OBJDIR)%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(TOOL_CXXFLAGS) ${COMP_OBJ}$@ $<

$(TOOLS): $(PIN_LIBNAMES) $(TOOL_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB)

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o
	${LINKER} -g $(TOOL_LDFLAGS) -I$(ZESTO_HOME) -L$(ZESTO_HOME) ${LINK_EXE}$@ $< ${TOOL_LPATHS} $(TOOL_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB) $(TOOL_LIBS)

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

install:
	cp $(TOOLS) $(COOL_INSTALL_DIR)
	git log -1 --format="%H" > $(COOL_INSTALL_DIR)/version 
	git diff --no-prefix > $(COOL_INSTALL_DIR)/diff 
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git log -1 --format="%H" > $(COOL_INSTALL_DIR)/repeater_version; fi 
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git diff --no-prefix > $(COOL_INSTALL_DIR)/repeater_diff; fi 
