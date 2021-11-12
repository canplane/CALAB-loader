#include "../branch.c"




#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define ARRAY_SIZE 1000000
int a[ARRAY_SIZE];
int b[ARRAY_SIZE];

int main()
{
  // signal(SIGSEGV, SIG_DFL);
  // kill(getpid(), SIGSEGV);
  for (int i = 0; i < ARRAY_SIZE/10; i++) {
    a[i] = 30;
    b[i] = 30;
  }
  for (int i = 0; i < ARRAY_SIZE/10; i++) {
    if (a[i] != 30) {
      printf("error! a[%d] != 30 \n", i);
      exit(-1);
    }
    if (b[i] != 30) {
      printf("error! a[%d] != 30 \n", i);
      exit(-1);
    }
  }
  // a[7001] = 30;
  printf("Everything checks out!\n");

  int fd = open("/proc/self/status", O_RDONLY);
  char data[4096];
  read(fd, &data, 4096);
  close(fd);
  printf("%s", data);

  return_to_loader(27);
}
