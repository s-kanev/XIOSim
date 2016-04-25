all: piii/fib piii/roi piii/repl piii/segf piii/step \
		 piii/rep piii/chase piii/prefetch piii/ignore piii/time \
		 piii/loop piii/rdtsc \
		 k8/fib k8/roi k8/repl k8/segf k8/step k8/rep k8/chase \
		 k8/prefetch k8/ignore k8/time k8/loop k8/rdtsc k8/atomics


piii/fib: fib.cpp
	g++ -m32 -std=c++1y -static -O0 -o $@ $<
piii/roi: roi.cpp
	g++ -m32 -std=c++1y -mtune=atom -static -O0 -o $@ $<
piii/repl: repl.cpp
	g++ -m32 -std=c++1y -mtune=atom -static -O0 -o $@ $<
piii/segf: segf.cpp
	g++ -m32 -std=c++1y -static -O0 -o $@ $<
piii/step: step.cpp
	g++ -m32 -std=c++1y -mtune=atom -static -O3 -o $@ $<
piii/rep: rep.cpp
	g++ -m32 -std=c++1y -static -O0 -o $@ $<
piii/chase: prefetch.cpp
	g++ -m32 -std=c++1y -static -O1 -o $@ $<
piii/prefetch: prefetch.cpp
	g++ -m32 -std=c++1y -static -O1 -DPREFETCH -o $@ $<
piii/ignore: ignore.cpp
	g++ -m32 -std=c++1y -static -O0 -o $@ $<
piii/time: time.cpp
	g++ -m32 -O1 -falign-functions=64 -static -o $@ $<
piii/loop: loop.cpp
	g++ -m32 -O0 -static -o $@ $<
piii/rdtsc: rdtsc.cpp
	g++ -m32 -O1 -static -falign-functions=16 -o $@ $<

k8/fib: fib.cpp
	g++ -std=c++1y -static -O0 -o $@ $<
k8/roi: roi.cpp
	g++ -std=c++1y -mtune=atom -static -O0 -o $@ $<
k8/repl: repl.cpp
	g++ -std=c++1y -mtune=atom -static -O0 -o $@ $<
k8/segf: segf.cpp
	g++ -std=c++1y -static -O0 -o $@ $<
k8/step: step.cpp
	g++ -std=c++1y -mtune=atom -static -O3 -o $@ $<
k8/rep: rep.cpp
	g++ -std=c++1y -static -O0 -o $@ $<
k8/chase: prefetch.cpp
	g++ -std=c++1y -static -O1 -o $@ $<
k8/prefetch: prefetch.cpp
	g++ -std=c++1y -static -O1 -DPREFETCH -o $@ $<
k8/ignore: ignore.cpp
	g++ -std=c++1y -static -O0 -o $@ $<
k8/time: time.cpp
	g++ -O1 -falign-functions=64 -static -o $@ $<
k8/time_vdso: time.cpp
	g++ -O1 -falign-functions=64 -o $@ $<
k8/loop: loop.cpp
	g++ -O0 -static -o $@ $<
k8/rdtsc: rdtsc.cpp
	g++ -O1 -static -falign-functions=16 -o $@ $<
k8/atomics: atomics.cpp
	g++ -std=c++1y -O1 -static -falign-loops=64 -o $@ $<
