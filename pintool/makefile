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
PIN_HOME ?= /home/skanev/pin/pin-2.8-36111-gcc.3.4.6-ia32_intel64-linux
ZESTO_HOME = ..
TARGET = ia32
INSTALL_DIR = /group/brooks/xan/Molecool/bin/

##############################################################
#
# set up and include *.config files
#
##############################################################

PIN_KIT=$(PIN_HOME)
KIT=1
TESTAPP=$(OBJDIR)cp-pin.exe


TARGET_COMPILER?=gnu
ifdef OS
    ifeq (${OS},Windows_NT)
        TARGET_COMPILER=ms
    endif
endif

ifeq ($(TARGET_COMPILER),gnu)
    include $(PIN_HOME)/source/tools/makefile.gnu.config
    CXXFLAGS ?= -I$(PIN_HOME)/source/tools/InstLib -Wall -Werror -Wno-unknown-pragmas -g $(DBG) $(OPT)
    PIN=$(PIN_HOME)/pin
endif

ifeq ($(TARGET_COMPILER),ms)
    include $(PIN_HOME)/source/tools/makefile.ms.config
    DBG?=
    PIN=$(PIN_HOME)/pin.bat
endif


##############################################################
#
# Tools - you may wish to add your tool name to TOOL_ROOTS
#
##############################################################


TOOL_ROOTS = feeder_zesto

TOOL_OBJS = $(OBJDIR)fluffy.o

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))


##############################################################
#
# build rules
#
##############################################################

all: tools
debug:	CXXFLAGS += -DZESTO_PIN_DBG
debug:	tools
tools: $(OBJDIR) $(TOOLS) $(OBJDIR)cp-pin.exe
test: $(OBJDIR) $(TOOL_ROOTS:%=%.test)

$(ZESTO_HOME)/libsim.a:
	$(MAKE) -C $(ZESTO_HOME) lib

$(ZESTO_HOME)/mcpat/mcpat.a:
	$(MAKE) -C $(ZESTO_HOME)/mcpat lib

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

$(TOOLS): $(PIN_LIBNAMES) $(TOOL_OBJS) $(ZESTO_HOME)/libsim.a $(ZESTO_HOME)/mcpat/mcpat.a

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o 
	${PIN_LD} -g $(PIN_LDFLAGS) -I$(ZESTO_HOME) -L$(ZESTO_HOME) $(LINK_DEBUG) ${LINK_OUT} $@ $< ${PIN_LPATHS} $(TOOL_OBJS) $(ZESTO_HOME)/libsim.a $(ZESTO_HOME)/mcpat/mcpat.a $(PIN_LIBS) $(DBG) -g -lrt

## cleaning
clean:
	-rm -rf $(OBJDIR) *.out *.tested *.failed makefile.copy
	$(MAKE) -C $(ZESTO_HOME) clean
	$(MAKE) -C $(ZESTO_HOME)/mcpat clean

install:
	cp $(TOOLS) $(INSTALL_DIR)
