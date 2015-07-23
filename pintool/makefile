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

# 2. Change BOOST_HOME, CONFUSE_HOME if you have boost and libconfuse
#    installed in non-standard places.
#
#BOOST_HOME ?= /home/skanev/boost_1_54_0
#CONFUSE_HOME ?= /home/skanev

ZESTO_HOME = ..
CATCH_HOME =  Catch/single_include
# Since pin 2.13(4?) the version of InstLib having PinPoints suppot is no more,
# in favor of the shiny new PinPlay kit. Integrating with PinPlay is not surprisingly
# non-trivial for XIOSim, so for now we keep a local version of 'good ole InstLib'.
INSTLIB_HOME = InstLib

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

ifeq ($(wildcard $(PIN_ROOT)/source/tools/Config/unix.vars),)
    $(error Set PIN_ROOT to a valid Pin installation)
endif

##############################################################
#
# set up libraries and flags
#
##############################################################
CONFUSE_LIB = -lconfuse

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

ifeq ($(XED_ROOT),)
    $(error XED kid not found in PIN_ROOT)
endif
XED_LIBS = -lxed
XED_LPATHS = -L$(XED_ROOT)/lib
XED_CXXFLAGS = -I$(XED_ROOT)/include

# Now the actual compilation flags
CXXFLAGS ?= -g -std=c++11
CXXFLAGS += -Wno-unused-function -Wno-unknown-pragmas -Wno-unused-local-typedefs
CXXFLAGS += -DDEBUG -D_FILE_OFFSET_BITS=64
CXXFLAGS += $(CONFUSE_CXXFLAGS) $(BOOST_CXXFLAGS) $(XED_CXXFLAGS)
CXXFLAGS += -I$(INSTLIB_HOME)
CXXFLAGS += -I$(CATCH_HOME) -I$(ZESTO_HOME)
CXXFLAGS += -msse4a -mfpmath=sse

# additional debug flags
ifeq ($(DEBUG),1)
    CXXFLAGS += -DZESTO_PIN_DBG -DIPC_DEBUG -DPRINT_WAITS -DSCHEDULER_DEBUG
    CXXFLAGS += -DPARSE_DEBUG -DALLOCATOR_DEBUG -DCOMPRESSION_DEBUG -DMP_DEBUG
    CXXFLAGS += -DROI_DEBUG
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
UNIT_TESTS = $(OBJDIR)test_allocation $(OBJDIR)test_parse_speedup $(OBJDIR)test_speedup_models $(OBJDIR)test_handshake_compression $(OBJDIR)test_parse_configs $(OBJDIR)test_uop_cracker $(OBJDIR)test_decoder $(OBJDIR)test_stat_database

SPEEDUP_OBJS = $(OBJDIR)base_speedup_model.o $(OBJDIR)log_speedup_model.o $(OBJDIR)linear_speedup_model.o $(OBJDIR)linreg.o
ALLOCATOR_OBJS = $(SPEEDUP_OBJS) $(OBJDIR)penalty_allocator.o $(OBJDIR)local_opt_allocator.o $(OBJDIR)gang_allocator.o $(OBJDIR)base_allocator.o $(OBJDIR)optimization_targets.o

TIMING_OBJS = $(OBJDIR)timing_sim.o $(OBJDIR)lib.o $(OBJDIR)scheduler.o $(ALLOCATOR_OBJS) $(OBJDIR)multiprocess_shared.o  $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)BufferManagerConsumer.o $(OBJDIR)BufferManager.o

FEEDER_OBJS = $(OBJDIR)feeder_zesto.o $(OBJDIR)ildjit.o $(OBJDIR)parse_speedup.o $(OBJDIR)linreg.o $(OBJDIR)utils.o $(OBJDIR)sync_pthreads.o $(OBJDIR)syscall_handling.o $(OBJDIR)ignore_ins.o $(OBJDIR)mpkeys_impl.o $(OBJDIR)ipc_queues.o $(OBJDIR)multiprocess_shared.o $(OBJDIR)BufferManagerProducer.o $(OBJDIR)BufferManager.o $(OBJDIR)roi.o $(OBJDIR)replace_function.o

HARNESS_OBJS = $(OBJDIR)harness.o $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o

DECODING_OBJS = $(ZESTO_HOME)/decode.o $(ZESTO_HOME)/uop_cracker.o $(ZESTO_HOME)/fu.o $(ZESTO_HOME)/misc.o

UNIT_TEST_DEPS = $(ALLOCATOR_OBJS) $(SPEEDUP_OBJS) $(OBJDIR)parse_speedup.o $(OBJDIR)linreg.o $(OBJDIR)multiprocess_shared.o $(OBJDIR)ipc_queues.o $(OBJDIR)mpkeys_impl.o $(ZESTO_HOME)/zesto-config-params.o $(OBJDIR)lib.o $(DECODING_OBJS)

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
	$(CXX) -c $(CXXFLAGS) $(TOOL_CXXFLAGS) ${COMP_OBJ}$@ $<

##############################################################
#
# Linker rules
#
##############################################################
$(TIMING): $(TIMING_OBJS) $(SIM_LIB) $(MCPAT_LIB) $(DRAMSIM_LIB) $(REPEATER_LIB)
	${LINKER} -g ${LINK_EXE}$@ ${XED_LPATHS} $^ $(BOOST_LIBS) $(XED_LIBS) $(CONFUSE_LIB)

$(FEEDER): $(FEEDER_OBJS) $(SIM_LIB)
	${LINKER} -g $(TOOL_LDFLAGS) ${LINK_EXE}$@ ${TOOL_LPATHS} $^ $(TOOL_OBJS) $(BOOST_LIBS) $(TOOL_LIBS)

$(HARNESS): $(HARNESS_OBJS)
	${LINKER} -g ${LINK_EXE}$@ ${CONFUSE_LPATHS} $^ $(BOOST_LIBS) $(CONFUSE_LIB)

$(UNIT_TESTS): % : %.o $(UNIT_TEST_DEPS) $(SIM_LIB) $(REPEATER_LIB) $(DRAMSIM_LIB) $(MCPAT_LIB)
	${LINKER} -g ${LINK_EXE}$@ ${XED_LPATHS} $^ ${CONFUSE_LPATHS} $(BOOST_LIBS) $(XED_LIBS) $(CONFUSE_LIB)

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

##############################################################
#
# Test harness
#
##############################################################
run_unit_tests: test
	$(foreach t,$(UNIT_TESTS),$t;)

run_unit_tests_junit: test
	$(foreach t,$(UNIT_TESTS),$t -r junit -o $(subst $(OBJDIR),$(ZESTO_HOME)/,$t.xml;))

run_integration_tests: all
	cd $(ZESTO_HOME); py.test scripts/run_tests.py

run_integration_tests_junit: all
	cd $(ZESTO_HOME); py.test --junitxml integration.xml scripts/run_tests.py
