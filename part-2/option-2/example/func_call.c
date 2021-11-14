#include <stdio.h>

int foobar (int a, int b)
{
    printf("Foobar Foobar %d\n", a+b);
    return 0;
}

int main(int argc, char **argv)
{
    foobar (3, 4);

    return 102;
}
