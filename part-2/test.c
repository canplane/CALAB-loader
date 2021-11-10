#include <stdio.h>
#include <stdlib.h>



#include <setjmp.h>

void return_to_loader()
{
    void *addr;
    char *addr_str;

    if ((addr_str = getenv("MY_LOADER_JMPBUF")) == NULL) {
        fprintf(stderr, "%s(): The program is not executed on the loader program.\n", __func__);
        return;
    }
    if (sscanf(addr_str, "%p", &addr) != 1) {
        fprintf(stderr, "%s(): Invalid environment variable: \"MY_LOADER_JMPBUF=%s\"\n", __func__, addr_str);
        return;
    }
    longjmp(*(jmp_buf *)addr, 1);

    fprintf(stderr, "%s(): This is never printed.\n", __func__);
}



int a;              // .bss: unintialized static variables
int b = 2;          // .data: initialized static variables
char *s = "foo";    // .rodata

int main()
{
    printf("Hello World!\n");
    
    char *p = malloc(0x100000);
    printf("malloc(0x100000): %p\n", p);

    /*
    char *q = NULL;
    printf("Can I assign a value to nullptr?\n");
    *q = 'a';
     */

    return_to_loader();
}
// echo $? -> 1
