#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>


extern char **environ;	// get environment variables

int	main(int argc, char *argv[])
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