#include "../branch.c"



#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("B - 1\n");

    yield();                    // yield
    
    printf("B - 2\n");

    yield();                    // yield
    
    printf("B - 3\n");

    printf("B: return 252\n");
    return_to_loader(252);      // exit: $ echo $? -> 252
}
