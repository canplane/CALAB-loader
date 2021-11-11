#ifndef			__BRANCH_C__
#define			__BRANCH_C__



#include		<stdio.h>
#include 		<stdlib.h>



#define			ISR_ADDR_ENV_VARNAME		"CALAB_LOADER__ISR_ADDR"

int __call_ISR(int code)
{
	void *func_addr;
	char *func_addr_str;

	if ((func_addr_str = getenv(ISR_ADDR_ENV_VARNAME)) == NULL) {
		fprintf(stderr, "%s(): The program is not executed on the loader program.\n", __func__);
		return 0;
	}
	if (sscanf(func_addr_str, "%p", &func_addr) != 1) {
		fprintf(stderr, "%s(): Invalid environment variable: \"%s=%s\"\n", __func__, ISR_ADDR_ENV_VARNAME, func_addr_str);
		return 0;
	}

	int (*fp)(int) = (int (*)(int))func_addr;
	return fp(code);
}

int return_to_loader()	{ return __call_ISR(1); }
int yield()				{ return __call_ISR(2); }



#endif