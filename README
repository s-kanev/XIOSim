XIOSim
======
XIOSim is a detailed user-mode microarchitectural simulator for the x86 architecture.
It has detailed models for in-order (Atom-like) and out-of-order (Nehalem-like) cores
and tightly-integrated power models. XIOSim supports multiprogram and multithreaded
execution, regions-of-interest (including SimPoints). It runs at 10s KIPS per simulated
core and uses cores on the simulation host to speed up muliticore simulation
(fastest runs use 2x the number of simulated cores).

XIOSim builds up on and integrates a significant amount of others' work:

- The out-of-order performance model from [Zesto](http://zesto.cc.gatech.edu/).
- The [Pin](http://www.pintool.org) binary instrumentation engine.
- The power models from [McPAT](http://www.hpl.hp.com/research/mcpat/).
- The DRAM models from [DRAMSim2](http://wiki.umd.edu/DRAMSim2/index.php/Main_Page).

### Dependences ###
- Pin kit version 2.14+ [Download](http://www.pintool.org/downloads.html)
- Recent version of GCC including C++11 support (4.7+ is ok)
- Boost 1.54+
- libconfuse
- (integration tests only) Python and py.test

To install dependent libraries on an Ubuntu system:

~~~
sudo apt-get install libboost-all-dev libconfuse-dev:i386
~~~

### Try it out ###
~~~
export PIN_ROOT=</path/to/your/pin/installation>
cd pintool
make
./run.sh
~~~

### Build status ###
[![Build Status](http://ci.xiosim.org:8080/buildStatus/icon?job=XIOSim)](http://ci.xiosim.org:8080/job/XIOSim/)

### Configuration and flags ###
Most simulated parameters are contained in a configuration file.
Feel free to browse [a Nehalem-based machine](config/Nconfuse.cfg) to get a feel for them.

Simulated applications are specified in their own configuration file.
Check out [how that looks](pintool/benchmarks.cfg).
You can add as many as you want for a single multiprogram simulation.

A few parameters (mostly related to regions-of-interest) are
passed as command-line flags (between the -t and -s switches).
For example:

~~~
-ppfile test.pp     # to use a PinPoints file.
-skip 10000         # to fast-forward the first 10,000 instructions.
-length 10000       # to only simmulate 10,000 instructions.
-parsec             # to properly use the Parsec suite region-of-interest hooks (ROI).
~~~

### ISA support ####
The simulator supports user-mode, 32-bit instructions. Some basic SSE instructions
are supported for doing floating point, but by no means the whole extension set
(a warning is printed if more than 2% instructions are unsupported.
In that case, ping Svilen to add your fancy instructions).
Support for 64-bit mode, SSEx and/or AVX is planned.
