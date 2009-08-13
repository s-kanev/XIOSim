#include <stdio.h>

int fib(int n)
{
  if(n<=1)
    return 1;
  else
    return fib(n-1)+fib(n-2);
}

main()
{
  int i;
  for(i=0;i<16;i++)
    fprintf(stderr,"fib(%d) = %d\n",i,fib(i));
  return 0;
}
