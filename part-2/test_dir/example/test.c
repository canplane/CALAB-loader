#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

void return_to_loader(...)
{
}

int main()
{
    printf("My PID is: %ld\n", (long)getpid());

    // allocates large array
    char *buffer = (char *) malloc(sizeof(char) * 500000);
    for (int i = 0; i < 500000; i++) {
        buffer[i] = 'c';
    }

    // write to random
    int fd = open("/dev/urandom", O_RDONLY);
    char data[4096];
    read(fd, &data, 4096);
    close(fd);
    fd = open("/dev/null", O_WRONLY);
    write(fd, &data, 4096);
    close(fd);

    int fd2 = open("/proc/self/status", O_RDONLY);
    char data2[4096];
    read(fd2, &data2, 4096);
    close(fd2);
    printf("%s", data2);

    return_to_loader(...);
}
