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
USER_LOCAL ?= /home/samxi/local/
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
    CXXFLAGS ?= -I$(PIN_HOME)/source/tools/InstLib  -Wno-unknown-pragmas -g $(DBG) $(OPT)
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
LIBCONFUSE_FLAGS = -I$(USER_LOCAL)include -L$(USER_LOCAL)lib

##############################################################
#
# Tools - you may wish to add your tool name to TOOL_ROOTS
#
##############################################################


TOOL_ROOTS = timing_sim

TOOL_OBJS = $(OBJDIR)lib.o $(OBJDIR)utils.o $(OBJDIR)scheduler.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)multiprocess_shared.o $(OBJDIR)BufferManagerConsumer.o $(OBJDIR)BufferManager.o

HARNESS_OBJS = $(OBJDIR)harness.o $(OBJDIR)mpkeys_impl.o

FEEDER_OBJS = $(ZESTO_HOME)/buffer.o $(OBJDIR)ildjit.o $(OBJDIR)fluffy.o $(OBJDIR)utils.o $(OBJDIR)sync_pthreads.o $(OBJDIR)parsec.o $(OBJDIR)syscall_handling.o $(OBJDIR)ignore_ins.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)multiprocess_shared.o $(OBJDIR)BufferManagerProducer.o $(OBJDIR)BufferManager.o

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))

STATIC_LIBS = mpkeys
SHM_LDFLAGS = -ldl

##############################################################
#
# build rules
#
##############################################################
all : CXXFLAGS += -DDEBUG -D_FILE_OFFSET_BITS=64 -std=c++11
all : CXXFLAGS += -I$(BOOST_INCLUDE) $(LIBCONFUSE_FLAGS) $(BOOST_LDFLAGS) $(SHM_LDFLAGS)
all: tools harness $(OBJDIR)feeder_zesto.so timing_wait
harness: CXXFLAGS += -I$(BOOST_INCLUDE) $(LIBCONFUSE_FLAGS) -std=c++11
debug:	CXXFLAGS += -DZESTO_PIN_DBG -D_FILE_OFFSET_BITS=64 -std=c++11
debug : CXXFLAGS += -I$(BOOST_INCLUDE)
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

static_libs: $(STATIC_LIBS)_impl.cpp
	$(CXX) -fPIC -shared $(STATIC_LIBS).h -o $(STATIC_LIBS).so
#$(CXX) $(STATIC_LIBS).a $(OBJDIR)lib$(STATIC_LIBS).o

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o : %.cpp
	$(CXX) -c -I$(BOOST_INCLUDE) $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(PIN_LIBNAMES) $(TOOL_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB)

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o
	${PIN_LD} -g $(PIN_LDFLAGS) -I$(ZESTO_HOME) -L$(ZESTO_HOME) -I$(BOOST_INCLUDE) $(LINK_DEBUG) ${LINK_OUT} $@ $< ${PIN_LPATHS} $(TOOL_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB) $(PIN_LIBS) $(DBG) -lrt -pthread 

$(OBJDIR)feeder_zesto.so: $(FEEDER_OBJS) $(OBJDIR)feeder_zesto.o
	${PIN_LD} -g $(PIN_LDFLAGS) -I$(ZESTO_HOME) -I$(BOOST_INCLUDE) $(LINK_DEBUG) ${LINK_OUT} $@ ${PIN_LPATHS} $^ $(PIN_LIBS) $(DBG) -lrt -pthread 

harness: $(HARNESS_OBJS)
	${PIN_LD} -g $(CXXFLAGS) ${SHM_LDFLAGS} $(LINK_DEBUG) ${LINK_OUT} $@ $^ -pthread -lrt -lconfuse

timing_wait: timing_wait.cpp
	$(CXX) $(CXXFLAGS) ${OUTOPT} $@ $<

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
	-rm -rf harness *.failed makefile.copy

install:
	cp $(TOOLS) $(COOL_INSTALL_DIR)
	git log -1 --format="%H" > $(COOL_INSTALL_DIR)/version
	git diff --no-prefix > $(COOL_INSTALL_DIR)/diff
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git log -1 --format="%H" > $(COOL_INSTALL_DIR)/repeater_version; fi
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git diff --no-prefix > $(COOL_INSTALL_DIR)/repeater_diff; fi
