#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

extern int errno;
extern char **environ;	// get environment variables

int my_execve(const char *path, char *const argv[], char *const envp[])
{
	int fd;
    char *addr;
    struct stat statbuf;

	// open file and set statbuf
	if (stat(argv[0], &statbuf) == -1) {
        fprintf(stderr, "Error: stat: %s\n", strerror(errno));
        exit(1);
    }
    if ((fd = open(argv[0], O_RDWR | O_CREAT)) == -1) {
        fprintf(stderr, "Error: open: %s\n", strerror(errno));
        exit(1);
    }
	// mmap
	addr = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);
    if (addr == MAP_FAILED) {
        perror("Error: mmap\n");
        exit(1);
    }

    int WORD = 16;
    for (int offset = 0; offset < statbuf.st_size; offset++) {
        if (offset % WORD == 0) {
            printf("\n%06d:", offset);
        }
        if (32 < addr[offset] && addr[offset] < 127) {
            printf(" %2c", addr[offset]);
        }
        else {
            printf(" %02x", (int)addr[offset] & 0x000000ff);
        }
    }
    printf("\n");
    printf("%lld\n", statbuf.st_size);

	// munmap and close file
    if (munmap(addr, 40) == -1) {
        perror("Error: munmap\n");
        exit(1);
    }
    close(fd);

	return 0;
}
#define execve my_execve

int main(int argc, char *argv[])
{
	char	**new_argv;

	if (argc < 2) {
        fprintf(stderr, "Usage: %s file [args ...]\n", argv[0]);
        exit(1);
    }

	new_argv = &argv[1];
	if (execve(new_argv[0], new_argv, environ) == -1) {
		fprintf(stderr, "Cannot execute the program '%s': %s\n", new_argv[0], strerror(errno));
		exit(1);
	}

	printf("This is not to be printed\n");
	return 0;
}