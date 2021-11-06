#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void segfault_sigaction(int signo, siginfo_t *si, void *arg)
{
    printf("Caught segfault at address %p\n", si->si_addr);
    //exit(0);
}

int main(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_sigaction;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    sigaction(SIGSEGV, &sa, NULL);

    //int *foo = NULL;

    /* Cause a seg fault */
    char s[] = "Hello";
    char *p = s;
    printf("%c", p[12452132139]);

    //*foo = 1;

    printf("foo\n");

    return 0;
}