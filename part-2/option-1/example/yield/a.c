#include "../../interrupt.c"




#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("A - 1\n");

    yield();                    // yield
    
    printf("A - 2\n");

    yield();                    // yield
    
    printf("A - 3\n");

    printf("A: return 11\n");
    return_to_loader(11);       // exit(11)
}
