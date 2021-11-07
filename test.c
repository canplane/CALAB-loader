#include <stdio.h>

int a;              // .bss: unintialized static variables
int b = 2;          // .data: initialized static variables
char *s = "foo";    // .rodata

int main()
{
    printf("Hello World!\n");

    return 0;
}