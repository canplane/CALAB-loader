#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("%p\n", brk(0));
    printf("%p\n", sbrk(1000));
    printf("%p\n", sbrk(1000));
}
