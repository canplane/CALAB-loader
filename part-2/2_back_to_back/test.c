#include <stdio.h>
#include <stdlib.h>

#include "branch.c"

int a;
int b = 2;
char *s = "foo";

int main()
{
    printf("Hello World!\n");

    yield();                    // yield
    
    char *p = malloc(0x100000);
    printf("malloc(0x100000): %p\n", p);

    /*
    char *q = NULL;
    printf("Can I assign a value to nullptr?\n");
    *q = 'a';
     */

    return_to_loader(254);      // exit: $ echo $? -> 254
}
