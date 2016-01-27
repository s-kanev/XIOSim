/* Test for various timing-related syscalls (times, gettimeofday).
 *
 * Simulator should return simulated time instead of host time.
 *
 */

#include <sys/time.h>
#include <sys/times.h>
#include <stdio.h>
#include <unistd.h>

extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

const int ITER = 1000000;

double times_test() {
  struct tms start, end;
  clock_t start_tick, end_tick;
  start_tick = times(&start);
  printf("starting seconds: %d\n", start.tms_utime);

  unsigned long long counter = 0;
  while (counter < ITER) {
      counter++;
  }
  end_tick = times(&end);

  printf("Counter value = %d\n", counter);
  printf("Elapsed seconds: %d\n", end.tms_utime);
  printf("Elapsed clock ticks: %llu\n", end_tick - start_tick);
  return (end.tms_utime - start.tms_utime);
}

unsigned loop() {
  unsigned counter = 0;
  __asm__ __volatile ("1: addl $1, %0;"
                      "cmpl %2, %1;"
                      "jb 1b;"
                      : "=a"(counter)
                      : "0"(counter), "i"(ITER)
                      : "memory");
  return counter;
}

double gettimeofday_test() {
  struct timeval start, end;
  gettimeofday(&start, NULL);

  unsigned counter = loop();

  gettimeofday(&end, NULL);

  /* 1M iterations of a 3 instruction loop -> 3M instructions.
   * With pipelining we get 1 cycles per iteration for a total of 1M cycles.
   * Default NHM config is 3.2GHz, so we should expect a time difference
   * of around 1M / 3.2GHz = 317us.
   */
  printf("Counter value = %d\n", counter);
  return (end.tv_sec - start.tv_sec) + 1.0*(end.tv_usec - start.tv_usec)/(1e6);
}

int main() {
  // For now, just test gettimeofday() because we haven't completed times().
  xiosim_roi_begin();
  double elapsed = gettimeofday_test();
  xiosim_roi_end();
  printf("Elapsed: %3.8f sec\n", elapsed);
  return 0;
}
