#include <stdio.h>
#include <stdlib.h>

#include "branch.c"

int a;              // .bss: unintialized static variables
int b = 2;          // .data: initialized static variables
char *s = "foo";    // .rodata

int main()
{
    printf("Hello World!\n");

    // yield
    yield();
    
    char *p = malloc(0x100000);
    printf("malloc(0x100000): %p\n", p);

    /*
    char *q = NULL;
    printf("Can I assign a value to nullptr?\n");
    *q = 'a';
     */

    // exit
    return_to_loader(254);      // $ echo $? -> 254
}
