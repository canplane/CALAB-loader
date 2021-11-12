#include "../../interrupt.c"




#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("C - 1\n");

    yield();                    // yield
    
    printf("C - 2\n");

    yield();                    // yield
    
    printf("C - 3\n");

    printf("C: return 33\n");
    return_to_loader(33);       // exit(33)
}
