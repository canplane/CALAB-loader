#include "../interrupt.c"




#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv, char** envp)
{
  printf("env:%s\n", getenv("PATH"));
  printf("size of void pointer: %zu \n", sizeof(void *));
  printf("size of argc: %zu \n", sizeof(argc));
  printf("size of char pointer: %zu \n", sizeof(char*));
  printf("size of Elf64_auxv_t: %zu \n", sizeof(Elf64_auxv_t));

  int i;
  printf("%d\n", argc);
  for (i = 0; i < argc; i++)
  {
    printf("%s\n", argv[i]);
  }

  char** env;
  for (env = envp; *env != 0; env++)
  {
    char* thisEnv = *env;
    printf("%s\n", thisEnv);
  }

  Elf64_auxv_t *auxv;
  while (*envp++ != NULL);

  /*from stack diagram above: *envp = NULL marks end of envp*/
  i = 0;
  for (auxv = (Elf64_auxv_t *)envp; auxv->a_type != AT_NULL; auxv++)
    /* auxv->a_type = AT_NULL marks the end of auxv */
  {
    printf("%lu %u %u \n", (auxv->a_type), AT_PLATFORM, i++);
    if ( auxv->a_type == AT_PLATFORM)
      printf("AT_PLATFORM is: %s\n", ((char*)auxv->a_un.a_val));
  }

  int fd = open("/proc/self/status", O_RDONLY);
  char data[4096];
  read(fd, &data, 4096);
  close(fd);
  printf("%s", data);

  return_to_loader(104);
}
