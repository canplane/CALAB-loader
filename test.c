#include <stdio.h>
#include <stdlib.h>

int a;              // .bss: unintialized static variables
int b = 2;          // .data: initialized static variables
char s[] = "foo";   // .rodata

int main()
{
    printf("Hello World!\n");
    malloc(10000);
    printf("after malloc\n");
    return 0;
}