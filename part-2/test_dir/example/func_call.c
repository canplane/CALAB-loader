#include <stdio.h>

void return_to_loader (...)
{
}

int foobar (int a, int b)
{
    printf("Foobar Foobar %d\n", a+b);
    return 0;
}

int main(int argc, char **argv)
{
    foobar (3, 4);

    return_to_loader (...);
}
