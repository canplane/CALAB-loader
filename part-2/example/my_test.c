#include "../interrupt.c"




#include <stdio.h>
#include <stdlib.h>

int a;
int b = 2;
char *s = "foo";

int main()
{
    printf("Hello World!\n");

////////////////////////////////
    printf("call yield()\n");
    yield();                    // wait
////////////////////////////////

    char *p = malloc(0x100000);
    printf("malloc(0x100000): %p\n", p);

    /*
    char *q = NULL;
    printf("Can I assign a value to nullptr?\n");
    *q = 'a';   // segmentation fault
    printf("Success!\n");
     */
    
////////////////////////////////
     printf("call return_to_loader(222)\n");
    return_to_loader(222);      // exit with exit code 222
////////////////////////////////

    printf("return 111\n");
    return 111;
}