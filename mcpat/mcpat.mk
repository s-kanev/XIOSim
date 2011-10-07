SHELL = /bin/sh
.PHONY: all depend clean
.SUFFIXES: .cc .o

ifeq ($(TGT),lib)
  NTHREADS?= 1
else
  NTHREADS?= 4
endif


LIBS = 
INCS = -lm

ifeq ($(TAG),dbg)
  DBG = -Wall 
  OPT = -ggdb -g -O0 -DNTHREADS=1 -Icacti -I.
else
  DBG = 
  OPT = -O3 -msse2 -mfpmath=sse -DNTHREADS=$(NTHREADS) -Icacti -I.
  #OPT = -O0 -DNTHREADS=$(NTHREADS)
endif

#CXXFLAGS = -Wall -Wno-unknown-pragmas -Winline $(DBG) $(OPT) 
CXXFLAGS = -Wno-unknown-pragmas $(DBG) $(OPT) 
CXX = g++ -m32
CC  = gcc -m32
AR = ar rs
RANLIB = ranlib

VPATH = cacti

EXE_TARGET=mcpat
LIB_TARGET=mcpat.a

ifeq ($(TGT),lib)
  TARGET = $(LIB_TARGET)
else
  TARGET = $(EXE_TARGET)
endif


SRCS  = \
  Ucache.cc \
  XML_Parse.cc \
  arbiter.cc \
  area.cc \
  array.cc \
  bank.cc \
  basic_circuit.cc \
  basic_components.cc \
  cacti_interface.cc \
  component.cc \
  core.cc \
  crossbar.cc \
  decoder.cc \
  htree2.cc \
  interconnect.cc \
  io.cc \
  iocontrollers.cc \
  logic.cc \
  slave.cc \
  mat.cc \
  memoryctrl.cc \
  noc.cc \
  nuca.cc \
  parameter.cc \
  processor.cc \
  router.cc \
  sharedcache.cc \
  subarray.cc \
  technology.cc \
  uca.cc \
  wire.cc \
  xmlParser.cc 

ifeq ($(TGT),exe)
SRCS += \
  main.cc
else
  CXXFLAGS += -DPIN
endif

OBJS = $(patsubst %.cc,obj_$(TAG)/%.o,$(SRCS))

all: obj_$(TAG)/$(TARGET)
	cp -f obj_$(TAG)/$(TARGET) $(TARGET)

obj_$(TAG)/$(TARGET) : $(OBJS)
ifeq ($(TGT),exe)
	$(CXX) $(OBJS) -o $@ $(INCS) $(CXXFLAGS) $(LIBS) -pthread
else
	$(AR) $@ $(OBJS) 
	$(RANLIB) $@
endif

#obj_$(TAG)/%.o : %.cc
#	$(CXX) -c $(CXXFLAGS) $(INCS) -o $@ $<

obj_$(TAG)/%.o : %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	-rm -f *.o $(EXE_TARGET) $(LIB_TARGET)


