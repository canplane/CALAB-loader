#include "../../interrupt.c"




#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("B - 1\n");

    yield();                    // yield
    
    printf("B - 2\n");

    yield();                    // yield
    
    printf("B - 3\n");

    printf("B: return 22\n");
    return_to_loader(22);       // exit(22)
}
