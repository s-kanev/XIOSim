#include <stdio.h>
#include <pthread.h>

pthread_t worker_thread;

int fib(int n)
{
  if(n<=1)
    return 1;
  else
    return fib(n-1)+fib(n-2);
}

void* worker(void* arg)
{
  (void) arg;
  int i;
  for(i=0;i<16;i++)
    fprintf(stderr,"fib(%d) = %d\n",i,fib(i));
  return NULL;
}

main()
{
  int j;
  pthread_create(&worker_thread, NULL, worker, NULL);
  for(j=0; j<1000000; j++);
  worker(NULL);
  return 0;
}
