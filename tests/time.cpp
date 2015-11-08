/* Test for various timing-related syscalls (times, gettimeofday).
 *
 * Simulator should return simulated time instead of host time.
 *
 * Compile: g++ -O1 -m32 -static -o time time.c
 */

#include <sys/time.h>
#include <sys/times.h>
#include <stdio.h>
#include <unistd.h>

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

double gettimeofday_test() {
  struct timeval start, end;
  gettimeofday(&start, NULL);
  printf("Start time of day: %d s %d us\n", start.tv_sec, start.tv_usec);

  unsigned long long counter = 0;
  while (counter < ITER) {
      counter++;
  }
  gettimeofday(&end, NULL);

  /* 1M iterations of a 3 instruction loop -> 3M instructions. The branch is
   * highly predictable, and with pipelining we'll average 2 cycles per
   * iteration for a total of 2M cycles.  Default NHM config is 3.2GHz, so we
   * should expect around a time difference of 2M / 3.2GHz = 625us.
   */
  printf("Counter value = %d\n", counter);
  printf("End time of day: %d s %d us\n", end.tv_sec, end.tv_usec);
  return (end.tv_sec - start.tv_sec) + 1.0*(end.tv_usec - start.tv_usec)/(1e6);
}

int main() {
  // For now, just test gettimeofday() because we haven't completed times().
  double elapsed = gettimeofday_test();
  printf("Elapsed: %3.8f sec\n", elapsed);
  return 0;
}
