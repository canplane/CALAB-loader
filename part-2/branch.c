#ifndef			__BRANCH_C__
#define			__BRANCH_C__



#include		<stdio.h>
#include 		<stdlib.h>



#define			__CALAB_LOADER__ENVVARNAME__CALL		"__CALAB_LOADER__CALL"

#define			__CALAB_LOADER__CALL__exit				1
#define			__CALAB_LOADER__CALL__yield				2




// get ISR in loader
int (*__get_loader_call())(int, ...)
{
	void *ISR_addr;
	char *ISR_addr_str;

	if ((ISR_addr_str = getenv(__CALAB_LOADER__ENVVARNAME__CALL)) == NULL) {
		fprintf(stderr, "Warning: The program is not executed on the loader program.\n");
		return NULL;
	}
	if (sscanf(ISR_addr_str, "%p", &ISR_addr) != 1) {
		fprintf(stderr, "Warning: Invalid environment variable: \"%s=%s\"\n", __CALAB_LOADER__ENVVARNAME__CALL, ISR_addr_str);
		return NULL;
	}
	return (int (*)(int, ...))ISR_addr;
}

int return_to_loader(int exit_code)
{
	int (*loader_call)(int, ...) = __get_loader_call();
	if (!loader_call)
		exit(exit_code);
	
	return loader_call(__CALAB_LOADER__CALL__exit, exit_code);
}
int yield()
{
	int (*loader_call)(int, ...) = __get_loader_call();
	if (!loader_call)
		return 0;
	
	return loader_call(__CALAB_LOADER__CALL__yield);
}



#endif