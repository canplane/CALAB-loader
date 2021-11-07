#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    void *curr_brk = sbrk(0x0);
    printf("initial program break: %p\n", curr_brk);
    printf("%p\n", brk(curr_brk + 0x1000));
    malloc(0x1000);
    printf("after malloc: %p\n", sbrk(0x0));
    printf("%p\n", sbrk(0x1000));
    printf("%p\n", sbrk(-0x1000));
    printf("%p\n", sbrk(0x0));
}
