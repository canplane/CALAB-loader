#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int fd;
    char *addr;
    struct stat statbuf;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        exit(1);
    }

    if (stat(argv[1], &statbuf) == -1) {
        perror("Error: stat\n");
        exit(1);
    }
    if ((fd = open(argv[1], O_RDWR | O_CREAT)) == -1) {
        perror("Error: open\n");
        exit(1);
    }

    addr = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);
    if (addr == MAP_FAILED) {
        perror("Error: mmap\n");
        exit(1);
    }

    printf("%s\n", addr);

    if (munmap(addr, 40) == -1) {
        perror("Error: munmap\n");
        exit(1);
    }

    close(fd);

    return 0;
}
