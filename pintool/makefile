##############################################################
#
# User-specific configuration
#
##############################################################

#
# 1. Change PIN_HOME, BOOST_HOME, CONFUSE_HOME to point to the top-level directories
#    where Pin, boost and libconfuse are.
#    These can also be set on the command line or as environment variables
#
#PIN_HOME ?= /home/skanev/pin/2.12
#BOOST_HOME ?= /home/skanev/boost_1_54_0
#CONFUSE_HOME ?= /home/skanev
ZESTO_HOME = ..
CATCH_HOME =  Catch/single_include
TARGET = ia32

##############################################################
#
# set up and include *.config files
#
##############################################################

PIN_KIT=$(PIN_HOME)
KIT=1

ifeq ($(wildcard $(PIN_HOME)/source/tools/makefile.gnu.config),)
    $(error Set PIN_HOME to a valid Pin installation)
endif

include $(PIN_HOME)/source/tools/makefile.gnu.config
PIN=$(PIN_HOME)/pin

##############################################################
#
# set up flags
#
##############################################################
CONFUSE_LIBS = -lconfuse

ifneq ($(CONFUSE_HOME),)
    CONFUSE_LPATHS = -L$(CONFUSE_HOME)/lib
    CONFUSE_CXXFLAGS = -I$(CONFUSE_HOME)/include
else
    CONFUSE_LPATHS =
    CONFUSE_CXXFLAGS =
endif

# We use parts of boost that are in headers only, so no need to link with boost dynamic libs
BOOST_LIBS = -lrt -lpthread

ifneq ($(BOOST_HOME),)
    BOOST_CXXFLAGS = -I$(BOOST_HOME)
else
    BOOST_CXXFLAGS =
endif

SIMLIB_LPATHS = -L$(ZESTO_HOME)

# Now the actual compilation flags
CXXFLAGS ?= -g -std=c++11
CXXFLAGS += -Wall -Werror -Wno-unused-function -Wno-unknown-pragmas
CXXFLAGS += -DDEBUG -D_FILE_OFFSET_BITS=64
CXXFLAGS += $(CONFUSE_CXXFLAGS) $(BOOST_CXXFLAGS)
CXXFLAGS += -I$(PIN_HOME)/source/tools/InstLib
# For compiling test cases, specify the Catch unit testing framework directory.
CXXFLAGS += -I$(CATCH_HOME)

# additional debug flags
ifeq ($(DEBUG),1)
    CXXFLAGS += -DZESTO_PIN_DBG -DIPC_DEBUG -DPRINT_WAITS -DSCHEDULER_DEBUG
    CXXFLAGS += -DPARSE_DEBUG -DALLOCATOR_DEBUG
    CXXFLAGS += -O0
endif

##############################################################
#
# Executables and object files
#
##############################################################
TIMING = $(OBJDIR)timing_sim
FEEDER = $(OBJDIR)feeder_zesto.so
HARNESS = $(OBJDIR)harness
UNIT_TESTS = $(OBJDIR)test_allocation $(OBJDIR)test_parse_speedup

ALLOCATOR_OBJS = $(OBJDIR)penalty_allocator.o $(OBJDIR)local_opt_allocator.o $(OBJDIR)gang_allocator.o $(OBJDIR)base_allocator.o

TIMING_OBJS = $(OBJDIR)timing_sim.o $(OBJDIR)lib.o $(OBJDIR)scheduler.o $(ALLOCATOR_OBJS) $(OBJDIR)multiprocess_shared.o  $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)BufferManagerConsumer.o $(OBJDIR)BufferManager.o

FEEDER_OBJS = $(OBJDIR)feeder_zesto.o $(ZESTO_HOME)/buffer.o $(OBJDIR)ildjit.o $(OBJDIR)parse_speedup.o $(OBJDIR)fluffy.o $(OBJDIR)utils.o $(OBJDIR)sync_pthreads.o $(OBJDIR)parsec.o $(OBJDIR)syscall_handling.o $(OBJDIR)ignore_ins.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)ipc_queues.o $(OBJDIR)multiprocess_shared.o $(OBJDIR)BufferManagerProducer.o $(OBJDIR)BufferManager.o

HARNESS_OBJS = $(OBJDIR)harness.o $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o

UNIT_TEST_DEPS = $(ALLOCATOR_OBJS) $(OBJDIR)parse_speedup.o $(OBJDIR)multiprocess_shared.o $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o
UNIT_TEST_OBJS = $(UNIT_TESTS:%=%.o)

##############################################################
#
# build targets
#
##############################################################
all: $(OBJDIR) $(TIMING) $(HARNESS) $(FEEDER) $(UNIT_TESTS)
test: $(OBJDIR) $(UNIT_TESTS)

##############################################################
#
# Subcomponent rules
#
##############################################################
# Non-optional subcomponents
SIM_LIB = $(ZESTO_HOME)/libsim.a
MCPAT_LIB = $(ZESTO_HOME)/mcpat/mcpat.a

# Optional subcomponents
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

# debug build option for subcomponents
ifeq ($(DEBUG),1)
    LIB_BUILD = libd
else
    LIB_BUILD = lib
endif

# The recursive build rules for subcomponents
$(ZESTO_HOME)/libsim.a:
	$(MAKE) -C $(ZESTO_HOME) $(LIB_BUILD)

$(ZESTO_HOME)/DRAMSim2/libdramsim.a:
	$(MAKE) -C $(ZESTO_HOME)/DRAMSim2

$(ZESTO_HOME)/mcpat/mcpat.a:
	$(MAKE) -C $(ZESTO_HOME)/mcpat $(LIB_BUILD)

$(ZESTO_HOME)/mem-repeater/librepeater.a:
	$(MAKE) -C $(ZESTO_HOME)/mem-repeater $(LIB_BUILD)

$(OBJDIR):
	mkdir -p $(OBJDIR)

##############################################################
#
# Compilation rules
#
##############################################################
$(OBJDIR)%.o : $(OBJDIR)
$(OBJDIR)%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

##############################################################
#
# Linker rules
#
##############################################################
$(TIMING): $(TIMING_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB)
	${PIN_LD} -g ${LINK_OUT} $@ ${SIMLIB_LPATHS} $^ $(BOOST_LIBS)

$(FEEDER): $(FEEDER_OBJS)
	${PIN_LD} -g $(PIN_LDFLAGS) ${LINK_OUT} $@ ${PIN_LPATHS} $^ $(PIN_LIBS) $(BOOST_LIBS)

$(HARNESS): $(HARNESS_OBJS)
	${PIN_LD} -g ${LINK_OUT} $@ ${CONFUSE_LPATHS} $^ $(BOOST_LIBS) $(CONFUSE_LIBS)

$(UNIT_TESTS): % : %.o $(UNIT_TEST_DEPS)
	${PIN_LD} -g ${LINK_OUT} $@ $^ $(BOOST_LIBS)

##############################################################
#
# Cleaning and installation
#
##############################################################
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
	cp $(TIMING) $(COOL_INSTALL_DIR)
	cp $(FEEDER) $(COOL_INSTALL_DIR)
	cp $(HARNESS) $(COOL_INSTALL_DIR)
	git log -1 --format="%H" > $(COOL_INSTALL_DIR)/version
	git diff --no-prefix > $(COOL_INSTALL_DIR)/diff
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git log -1 --format="%H" > $(COOL_INSTALL_DIR)/repeater_version; fi
	if [ -n "$(REPEATER_LIB)" ]; then cd ../mem-repeater/ ; git diff --no-prefix > $(COOL_INSTALL_DIR)/repeater_diff; fi
