#include <stdio.h>
#include <stdlib.h>

int a;
int b = 2;
char *s = "foo";

int main()
{
    printf("Hello World!\n");
    
    char *p = malloc(0x100000);
    printf("malloc(0x100000): %p\n", p);

    /*
    char *q = NULL;
    printf("Can I assign a value to nullptr?\n");
    *q = 'a';   // segmentation fault
    printf("Success!\n");
     */
    
    return 254;
}