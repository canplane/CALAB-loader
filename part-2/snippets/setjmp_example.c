#include <stdio.h>
#include <setjmp.h>

static jmp_buf __ex_buf;

int i = 1;

void jmp(void) {
    printf("%d\n", i++);
    if (i < 10) {
        longjmp(__ex_buf, 1);
    } else {
        longjmp(__ex_buf, 2);
    }
}

int main()
{
    //printf("sizeof(jmp_buf) = %lu\n", sizeof(jmp_buf));

    if (setjmp(__ex_buf) == 2) {
        i = 0;
    }

    if (i != 0) {
        jmp();
    }

    return 0;
}