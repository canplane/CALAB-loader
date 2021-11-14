/* 
 * /part-2/interrupt.c
 * ----------------
 * CALAB Master Programming Project - (Part 2) Advanced user-level loader
 * Returning to the program loader
 * 
 * Sanghoon Lee (canplane@gmail.com)
 * 12 November 2021
 */


#ifndef			__INTERRUPT_C__
#define			__INTERRUPT_C__




#include		<stdio.h>
#include 		<stdlib.h>




/* font style */

#define			ERR_STYLE__								"\x1b[2m\x1b[3m"	// bold, italic
#define			UND_STYLE__								"\x1b[4m"			// underline
#define			INV_STYLE__								"\x1b[7m"			// inverse
#define			__ERR_STYLE								"\x1b[0m"			// reset to normal




/* loader call */

// the name of custom environment variable whose value is set to the address of function 'loader_ISR()'
#define			CALAB_LOADER__ENVVARNAME__CALL			"__CALAB_LOADER__CALL"

// loader call code
#define			CALAB_LOADER__CALL__exit				1
#define			CALAB_LOADER__CALL__yield				2


// get function pointer of 'loader_ISR()' in the loader program
int (*__get_loader_call(const char *__caller_name))(int, ...)
{
	void *ISR_addr;
	char *ISR_addr_str;

	if ((ISR_addr_str = getenv(CALAB_LOADER__ENVVARNAME__CALL)) == NULL) {
		fprintf(stderr, ERR_STYLE__"%s(): The program is not executed on the loader program.\n"__ERR_STYLE, __caller_name);
		return NULL;
	}
	if (sscanf(ISR_addr_str, "%p", &ISR_addr) != 1) {
		fprintf(stderr, ERR_STYLE__"%s(): Invalid environment variable: \"%s=%s\"\n"__ERR_STYLE, __caller_name, CALAB_LOADER__ENVVARNAME__CALL, ISR_addr_str);
		return NULL;
	}
	return (int (*)(int, ...))ISR_addr;
}

int return_to_loader(int exit_code)
{
	int (*loader_call)(int, ...) = __get_loader_call(__func__);		// 'loader_call()' is equal to 'loader_ISR()' in the loader program
	if (!loader_call)
		exit(exit_code);
	
	// call loader's ISR with code 1 (exit) and exit code
	return loader_call(CALAB_LOADER__CALL__exit, exit_code);
}
int yield()
{
	int (*loader_call)(int, ...) = __get_loader_call(__func__);		// 'loader_call()' is equal to 'loader_ISR()' in the loader program
	if (!loader_call)
		return 0;
	
	//call loader's ISR with code 2 (yield)
	return loader_call(CALAB_LOADER__CALL__yield);
}




#endif