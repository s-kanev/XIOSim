all: piii/fib piii/roi piii/repl piii/segf piii/step piii/rep piii/chase piii/prefetch piii/ignore k8/fib k8/roi k8/repl k8/segf k8/step k8/rep k8/chase k8/prefetch k8/ignore


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
