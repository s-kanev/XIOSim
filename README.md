XIOSim
======
XIOSim is a detailed user-mode microarchitectural simulator for the x86 architecture.
It has detailed models for in-order (Atom-like) and out-of-order (Nehalem-like) cores
and tightly-integrated power models. XIOSim supports multiprogram and multithreaded
execution, regions-of-interest (including SimPoints). It runs at 100s KIPS per simulated
core and uses cores on the simulation host to speed up multicore simulation
(fastest runs use 2x the number of simulated cores).

XIOSim builds up on and integrates a significant amount of others' work:

- The out-of-order performance model from [Zesto](http://zesto.cc.gatech.edu/).
- The [Pin](http://www.pintool.org) binary instrumentation engine.
- The power models from [McPAT](http://www.hpl.hp.com/research/mcpat/).
- The DRAM models from [DRAMSim2](http://wiki.umd.edu/DRAMSim2/index.php/Main_Page).

### Dependences ###
- Bazel 0.1 [Download](http://bazel.io/docs/install.html)
- (integration tests only) Python and py.test

XIOSim uses [bazel](http://bazel.io) for fetching and building dependences.
Check out [third_party](third_party/) for a complete list of libraries we use.

### Try it out ###
~~~
bazel build :xiosim
./run.sh
~~~

### Build status ###
[![Build Status](http://ci.xiosim.org:8080/buildStatus/icon?job=XIOSim)](http://ci.xiosim.org:8080/job/XIOSim/)

### Configuration and flags ###
Most simulated parameters are contained in a configuration file.
Feel free to browse [a Nehalem-based machine](xiosim/config/N.cfg) to get a feel for them.

Simulated applications are specified in their own configuration file.
Check out [how that looks](benchmarks.cfg).
You can add as many as you want for a single multiprogram simulation.

A few instruction feeder parameters (mostly related to regions-of-interest) are
passed as command-line flags (after the pintool -t switch).
For example:

~~~
-ppfile test.pp     # to use a PinPoints file.
-skip 10000         # to fast-forward the first 10,000 instructions.
-length 10000       # to only simmulate 10,000 instructions.
~~~

### ISA support ####
The simulator supports user-mode, ia32 and x86_64 instructions. If you want to
simulate 32-bit applications, build with `bazel build --cpu=piii :xiosim`.

### License ###
XIOSim is under the BSD license, unless otherwise noted. In-tree third-party compoments
are under licences that are no more restrictive: Zesto is under a notice-type
license, copyright Georgia Tech; McPAT and CACTI are under a notice-type license,
copyright HP; ezOptionParser is under an MIT license; InstLib is under an
Intel Open Source license. Despite what some Zesto files mention, there is no
more code derived from SimpleScalar and covered by the SimpleScalar license.
