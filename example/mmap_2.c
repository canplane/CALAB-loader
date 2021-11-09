#include <stdio.h>
#include <sys/mman.h>

int main()
{
    char *ptr = mmap((void *)0x10000000, 4096,
            PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE | MAP_STACK | MAP_GROWSDOWN,
            -1, 0
        );
    printf("mmap %p\n", ptr);
    ptr[0] = 'a';
    printf("a\n");
    ptr[4095] = 'b';
    printf("b\n");
    ptr[4096] = 'c';
    printf("c\n");
}