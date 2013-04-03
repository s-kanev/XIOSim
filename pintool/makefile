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
TARGET = ia32

##############################################################
#
# set up and include *.config files
#
##############################################################

PIN_KIT=$(PIN_HOME)
KIT=1
TESTAPP=$(OBJDIR)cp-pin.exe


TARGET_COMPILER?=gnu
ifeq ($(TARGET_COMPILER),gnu)
    include $(PIN_HOME)/source/tools/makefile.gnu.config
    CXXFLAGS ?= -I$(PIN_HOME)/source/tools/InstLib -Wall -Werror -Wno-unknown-pragmas -g $(DBG) $(OPT)
    PIN=$(PIN_HOME)/pin
endif

SIM_LIB = $(ZESTO_HOME)/libsim.a
MCPAT_LIB = $(ZESTO_HOME)/mcpat/mcpat.a
REPEATER_LIB = $(ZESTO_HOME)/mem-repeater/librepeater.a

ifeq ($(wildcard $(ZESTO_HOME)/DRAMSim2/DRAMSim.h),)
    DRAMSIM_LIB =
else 
    DRAMSIM_LIB = $(ZESTO_HOME)/DRAMSim2/libdramsim.a
endif

##############################################################
#
# Tools - you may wish to add your tool name to TOOL_ROOTS
#
##############################################################


TOOL_ROOTS = feeder_zesto

TOOL_OBJS = $(OBJDIR)Buffer.o $(OBJDIR)BufferManager.o $(OBJDIR)ildjit.o $(OBJDIR)fluffy.o $(OBJDIR)lib.o $(OBJDIR)utils.o $(OBJDIR)sync_pthreads.o $(OBJDIR)scheduler.o $(OBJDIR)parsec.o

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))


##############################################################
#
# build rules
#
##############################################################
all : CXXFLAGS += -DDEBUG -D_FILE_OFFSET_BITS=64 -std=c++11
all: tools
debug:	CXXFLAGS += -DZESTO_PIN_DBG -D_FILE_OFFSET_BITS=64
debug:	tools
tools: $(OBJDIR) $(TOOLS) $(OBJDIR)cp-pin.exe
test: $(OBJDIR) $(TOOL_ROOTS:%=%.test)

$(ZESTO_HOME)/libsim.a:
	$(MAKE) -C $(ZESTO_HOME) lib

$(ZESTO_HOME)/DRAMSim2/libdramsim.a:
	$(MAKE) -C $(ZESTO_HOME)/DRAMSim2

$(ZESTO_HOME)/mcpat/mcpat.a:
	$(MAKE) -C $(ZESTO_HOME)/mcpat lib

$(ZESTO_HOME)/mem-repeater/librepeater.a:
	$(MAKE) -C $(ZESTO_HOME)/mem-repeater

MyPinTool.test: $(OBJDIR)cp-pin.exe
      $(MAKE) -k -C feeder_zesto PIN_HOME=$(PIN_HOME)

$(OBJDIR)cp-pin.exe:
	$(CXX) $(PIN_HOME)/source/tools/Tests/cp-pin.cpp $(APP_CXXFLAGS) -o $(OBJDIR)cp-pin.exe

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o : %.C
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(OBJDIR)%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(PIN_LIBNAMES) $(TOOL_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB)

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o 
	${PIN_LD} -g $(PIN_LDFLAGS) -I$(ZESTO_HOME) -L$(ZESTO_HOME) $(LINK_DEBUG) ${LINK_OUT} $@ $< ${PIN_LPATHS} $(TOOL_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB) $(PIN_LIBS) $(DBG)

## cleaning
clean:
	-rm -rf $(OBJDIR) *.out *.tested *.failed makefile.copy
	$(MAKE) -C $(ZESTO_HOME) clean
	$(MAKE) -C $(ZESTO_HOME)/mcpat clean
	if [ -n "$(DRAMSIM_LIB)" ]; then $(MAKE) -C $(ZESTO_HOME)/DRAMSim2 clean; fi
	$(MAKE) -C $(ZESTO_HOME)/mem-repeater clean

.PHONY: clean-tool
clean-tool:
	-rm -rf $(OBJDIR) *.out *.tested *.failed makefile.copy

install:
	cp $(TOOLS) $(COOL_INSTALL_DIR)
	git log -1 --format="%H" > $(COOL_INSTALL_DIR)/version 
	git diff --no-prefix > $(COOL_INSTALL_DIR)/diff 
