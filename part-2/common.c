#ifndef			__COMMON_C__
#define			__COMMON_C__



#include 		<stdio.h>
#include 		<stdlib.h>
#include 		<string.h>
#include		<stdbool.h>

#include 		<fcntl.h>
#include 		<unistd.h>
#include		<sys/mman.h>
#include 		<elf.h>

#include 		<errno.h>

extern int		errno;

#include		"queue.c"


#define			ERR_STYLE__								"\x1b[2m"
#define			INV_STYLE__								"\x1b[7m"
#define			__ERR_STYLE								"\x1b[0m"



/* memory layout */

// adapted from linux/fs/binfmt_elf.c
#define 		PAGE_SIZE								(size_t)(1 << 12)					// 4096 B = 4 KB
#define 		PAGE_FLOOR(_addr)						((_addr) & ~(unsigned long)(PAGE_SIZE - 1))			// ELF_PAGESTART
#define 		PAGE_OFFSET(_addr)						((_addr) & (PAGE_SIZE - 1))							// ELF_PAGEOFFSET
#define 		PAGE_CEIL(_addr)						(((_addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))		// ELF_PAGEALIGN



#define			__LOG2_THREAD_MAX_NUM					2		// This loader can load 4 programs at most.
#define			THREAD_MAX_NUM							(1 << __LOG2_THREAD_MAX_NUM)


#define			THREADS_LOW								0L
#define			THREADS_HIGH							0x40000000L
#define			THREAD_STRIDE							((THREADS_HIGH - THREADS_LOW) >> __LOG2_THREAD_MAX_NUM)
#define			THREAD_SPACE_LOW(_tid)					(THREADS_LOW + (_tid) * THREAD_STRIDE)
#define			THREAD_SPACE_HIGH(_tid)					(THREADS_LOW + ((_tid) + 1) * THREAD_STRIDE)


#define			__STACK_SIZE_WITH_PADDING				(1L << 23)
#define			PADDING									PAGE_SIZE

#define			STACK_SPACE_HIGH(_tid)					(THREAD_SPACE_HIGH(_tid) - PADDING)
#define			STACK_SPACE_LOW(_tid)					((THREAD_SPACE_HIGH(_tid) - __STACK_SIZE_WITH_PADDING) + PADDING)
#define			STACK_SIZE								(__STACK_SIZE_WITH_PADDING - PADDING << 1)		// 8 MB - 8 KB

#define			SEGMENT_SPACE_LOW(_tid)					THREAD_SPACE_LOW(_tid)
#define			SEGMENT_SPACE_HIGH(_tid)				(THREAD_SPACE_HIGH(_tid) - __STACK_SIZE_WITH_PADDING)


#define			PAGE_TABLE_LOW							THREADS_HIGH
#define			PAGE_TABLE_HIGH							(PAGE_TABLE_LOW + (1L << 20))		// 2^32 / 2^12 = 2^20 = 1 MB


#define			P_HEADERS_LOW							PAGE_TABLE_HIGH
#define			P_HEADERS_HIGH							(P_HEADERS_LOW + PAGE_SIZE << __LOG2_THREAD_MAX_NUM)
#define			P_HEADER_STRIDE							((P_HEADERS_HIGH - P_HEADERS_LOW) >> __LOG2_THREAD_MAX_NUM)
#define			P_HEADER(_tid)							(P_HEADERS_LOW + (_tid) * P_HEADER_STRIDE)


#define			BASE_ADDR								0x50000000L							// gcc -Ttext-segment=(BASE_ADDR) ...



/* thread */

#include		<stdarg.h>
#include		<setjmp.h>

#define			THREAD_STATE_NEW						0
#define			THREAD_STATE_RUN						1
#define			THREAD_STATE_WAIT						2
#define			THREAD_STATE_EXIT						3

#define			__CALAB_LOADER__ENVVARNAME__CALL		"__CALAB_LOADER__CALL"

#define			__CALAB_LOADER__CALL__exit				1
#define			__CALAB_LOADER__CALL__yield				2


typedef struct {
	int				state;
	jmp_buf			jmpenv;
	Elf64_Addr		page_tail, page_head;

	Elf64_Addr 		entry, sp;		// used at STATE_NEW
	int				exit_code;		// used at STATE_EXIT
} Thread;

jmp_buf			loader_jmpenv;
Thread 			thread[THREAD_MAX_NUM];

int 			current_thread_idx = -1;



// interrupt service routine: call by a thread
int loader_call(int code, ...)
{
	va_list ap;
	va_start(ap, code);
	
	int ret = 0;
	switch (code) {
		case __CALAB_LOADER__CALL__exit:
			thread[current_thread_idx].state = THREAD_STATE_EXIT;
			ret = thread[current_thread_idx].exit_code = va_arg(ap, int);

			if (!setjmp(thread[current_thread_idx].jmpenv))
				longjmp(loader_jmpenv, true);
			break;

		case __CALAB_LOADER__CALL__yield:
			thread[current_thread_idx].state = THREAD_STATE_WAIT;
			if (!setjmp(thread[current_thread_idx].jmpenv))
				longjmp(loader_jmpenv, true);
			break;

		default:
			fprintf(stderr, ERR_STYLE__"Warning: Invalid call code: %d\n"__ERR_STYLE, code);
			ret = -1;
			break;
	}

	va_end(ap);
	return ret;
}

int run(int thread_id)
{
	current_thread_idx = thread_id;

	if (!setjmp(loader_jmpenv)) {
		if (thread[thread_id].state == THREAD_STATE_NEW) {
			__asm__ __volatile__ (
				"movq $0, %%rdx\n\t"	// (%r9 = %rdx) executed at <_start>
				"movq %0, %%rsp\n\t"
				"jmp *%1"
				: : "a" (thread[thread_id].sp), "b" (thread[thread_id].entry)
			);
		}
		else
			longjmp(thread[thread_id].jmpenv, 1);		// context switch
	}

	current_thread_idx = -1;
	return 0;
}



Elf64_Addr create_stack(int thread_id, const char *argv[], const char *envp[], const char *envp_added[], const Elf64_Ehdr *ep)
{
	int i;


	/* allocate new stack space */

	fprintf(stderr, ERR_STYLE__"Mapping: user stack -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, STACK_SPACE_LOW(thread_id), STACK_SIZE);
	if (mmap((void *)STACK_SPACE_LOW(thread_id), STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Cannot allocate memory for user stack");
		exit(1);
	}


	/* get memory layout information */

	Elf64_auxv_t *auxv;
	int argc, envc, auxc;
	size_t argv_asciiz_space_size, envp_asciiz_space_size;

	int envc_added;
	size_t envp_added_asciiz_space_size;

	argv_asciiz_space_size = 0;								// argc, argument ASCIIZ strings
	for (i = 0; argv[i]; i++)
		argv_asciiz_space_size += strlen(argv[i]) + 1;
	argc = i;

	envp_asciiz_space_size = 0;								// envc, environment ASCIIZ strings
	for (i = 0; envp[i]; i++)
		envp_asciiz_space_size += strlen(envp[i]) + 1;
	envc = i;

	envp_added_asciiz_space_size = 0;						// 		+ additional envp to interact loader with thread
	for (i = 0; envp_added[i]; i++)
		envp_added_asciiz_space_size += strlen(envp_added[i]) + 1;
	envc_added = i;

	auxv = (Elf64_auxv_t *)(envp + envc + 1);				// auxc
	for (i = 0; auxv[i].a_type != AT_NULL; i++)
		;
	auxc = i;


	/* init stack pointer and resolve new addresses by information */

	Elf64_Addr sp;
	
	char **new_argv, **new_envp;
	Elf64_auxv_t *new_auxv;
	char *new_argv_asciiz_space, *new_envp_asciiz_space;

	sp = STACK_SPACE_HIGH(thread_id);						// bottom of stack

	sp -= sizeof(int64_t);									// end marker

	sp -= envp_asciiz_space_size;							// environment ASCIIZ string space
	sp -= envp_added_asciiz_space_size;						// 		+ additional envp to interact loader with thread
	new_envp_asciiz_space = (char *)sp;
	
	sp -= argv_asciiz_space_size;							// argument ASCIIZ string space
	new_argv_asciiz_space = (char *)sp;
	
	sp = sp & ~0xf;											// padding (0~16)
	
	sp -= (auxc + 1) * sizeof(Elf64_auxv_t);				// auxv
	new_auxv = (Elf64_auxv_t *)sp;
	
	sp -= (envc + 1) * sizeof(char *);						// envp
	sp -= envc_added * sizeof(char *);						// 		+ additional envp to interact loader with thread
	new_envp = (char **)sp;
	
	sp -= (argc + 1) * sizeof(char *);						// argv
	new_argv = (char **)sp;
	
	sp -= sizeof(int64_t);									// argc


	/* store informations to new addresses */

	Elf64_Addr _sp;
	size_t len;

	for (i = 0; i < auxc; i++) {							// auxv
		new_auxv[i] = auxv[i];
		switch (auxv[i].a_type) {
			case AT_PHNUM:			// 5: Number of program headers
				new_auxv[i].a_un.a_val = ep->e_phnum;
				//fprintf(stderr, ERR_STYLE__"Auxiliary vector modified: AT_PHNUM: %ld -> %ld\n"__ERR_STYLE, auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
			case AT_BASE:			// 7: Base address of interpreter
				new_auxv[i].a_un.a_val = 0;
				//fprintf(stderr, ERR_STYLE__"Auxiliary vector modified: AT_BASE: %#lx -> %#lx\n"__ERR_STYLE, auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
			case AT_ENTRY:			// 9: Entry point of program
				new_auxv[i].a_un.a_val = ep->e_entry;
				//fprintf(stderr, ERR_STYLE__"Auxiliary vector modified: AT_ENTRY: %#lx -> %#lx\n"__ERR_STYLE, auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
		}
	}

	_sp = (Elf64_Addr)new_envp_asciiz_space;				// envp, environment ASCIIZ strings
	for (i = 0; i < envc; i++) {
		memcpy((void *)_sp, envp[i], len = strlen(envp[i]) + 1);
		new_envp[i] = (void *)_sp, _sp += len;
	}
	for (i = 0; i < envc_added; i++) {						// 	+ my_var
		memcpy((void *)_sp, envp_added[i], len = strlen(envp_added[i]) + 1);
		new_envp[envc + i] = (void *)_sp, _sp += len;
	}

	_sp = (Elf64_Addr)new_argv_asciiz_space;				// argv, argument ASCIIZ strings
	for (i = 0; i < argc; i++) {
		memcpy((void *)_sp, argv[i], len = strlen(argv[i]) + 1);
		new_argv[i] = (void *)_sp, _sp += len;
	}

	*(int64_t *)sp = (int64_t)argc;							// argc


	/* return stack pointer */

	return sp;
}



/* for debugging */
/*
void print_e_header(const Elf64_Ehdr *ep)
{
	fprintf(stderr, "{\n");
	fprintf(stderr, "\tunsigned char \te_ident[16] \t= \"%s\"\n", ep->e_ident);		// ELF identification
    fprintf(stderr, "\tElf64_Half \te_type \t\t= %#x\n", ep->e_type);				// Object file type
    fprintf(stderr, "\tElf64_Half \te_machine \t= %#x\n", ep->e_machine);			// Machine type
    fprintf(stderr, "\tElf64_Word \te_version \t= %u\n", ep->e_version);			// Object file version
    fprintf(stderr, "\tElf64_Addr \te_entry \t= %#lx\n", ep->e_entry);				// Entry point address
	fprintf(stderr, "\tElf64_Off \te_phoff \t= %#lx\n", ep->e_phoff);				// Program header offset
	fprintf(stderr, "\tElf64_Off \te_shoff \t= %#lx\n", ep->e_shoff);				// Section header offset
    fprintf(stderr, "\tElf64_Word \te_flags \t= %#x\n", ep->e_flags);				// Processor-specific flags
    fprintf(stderr, "\tElf64_Half \te_ehsize \t= %u\n", ep->e_ehsize);				// ELF header size (equal to sizeof(Elf64_Ehdr))
    fprintf(stderr, "\tElf64_Half \te_phentsize \t= %u\n", ep->e_phentsize);		// Size of program header entry
	fprintf(stderr, "\tElf64_Half \te_phnum \t= %u\n", ep->e_phnum);				// Number of program header entries
    fprintf(stderr, "\tElf64_Half \te_shentsize \t= %u\n", ep->e_shentsize);		// Size of section header entry
    fprintf(stderr, "\tElf64_Half \te_shnum \t= %u\n", ep->e_shnum);				// Number of section header entries
    fprintf(stderr, "\tElf64_Half \te_shstrndx \t= %u\n", ep->e_shentsize);			// Section name string table index
	fprintf(stderr, "}\n");
}

void print_p_header(const Elf64_Phdr *pp)
{
	fprintf(stderr, "{\n");
    fprintf(stderr, "\tElf64_Word \tp_type \t\t= %#x\n", pp->p_type);				// Type of segment
    fprintf(stderr, "\tElf64_Word \tp_flags \t= %#x\n", pp->p_flags);				// Segment attributes
    fprintf(stderr, "\tElf64_Off \tp_offset \t= %#lx\n", pp->p_offset);				// Offset in file
    fprintf(stderr, "\tElf64_Addr \tp_vaddr \t= %#lx\n", pp->p_vaddr);				// Virtual address in memory
    //fprintf(stderr, "\tElf64_Addr \tp_paddr \t= %#lx\n", pp->p_paddr);			// Reserved
    fprintf(stderr, "\tElf64_Xword \tp_filesz \t= %#lx\n", pp->p_filesz);			// Size of segment in file
    fprintf(stderr, "\tElf64_Xword \tp_memsz \t= %#lx\n", pp->p_memsz);				// Size of segment in memory
    fprintf(stderr, "\tElf64_Xword \tp_align \t= %#lx\n", pp->p_align);				// Alignment of segment
	fprintf(stderr, "}\n");
}

void print_stack(const char **argv)
{
	char **envp;
	Elf64_auxv_t *auxv;
	int argc, envc, auxc;

	for (int i = 0; ; i++) {
		fprintf(stderr, "argv[%d] <%p> -> \"%s\" <%p>\n", i, &argv[i], argv[i], argv[i]);
		if (!argv[i]) {
			argc = i;
			break;
		}
	}
	printf("argc = %d\n", argc);

	envp = (char **)(argv + argc + 1);							// envp
	for (int i = 0; ; i++) {
		fprintf(stderr, "envp[%d] <%p> -> \"%s\" <%p>\n", i, &envp[i], envp[i], envp[i]);
		if (!envp[i]) {
			envc = i;
			break;
		}
	}
	printf("envc = %d\n", envc);

	auxv = (Elf64_auxv_t *)(envp + envc + 1);					// auxc
	for (int i = 0; ; i++) {
		fprintf(stderr, "auxv[%d] <%p> = ", i, &auxv[i]);
		switch (auxv[i].a_type) {
			case AT_NULL:			// 0: End of vector
				fprintf(stderr, "AT_NULL, (ignored)\n");
				break;
			case AT_IGNORE:			// 1: Entry should be ignored
				fprintf(stderr, "AT_IGNORE, (ignored)\n");
				break;
			case AT_EXECFD:			// 2: File descriptor of program
				fprintf(stderr, "AT_EXECFD, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_PHDR:			// 3: Program headers for program
				fprintf(stderr, "AT_PHDR, %#lx\n", auxv[i].a_un.a_val);		// .a_ptr
				break;
			case AT_PHENT:			// 4: Size of program header entry
				fprintf(stderr, "AT_PHENT, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_PHNUM:			// 5: Number of program headers
				fprintf(stderr, "AT_PHNUM, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_PAGESZ:			// 6: System page size
				fprintf(stderr, "AT_PAGESZ, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_BASE:			// 7: Base address of interpreter
				fprintf(stderr, "AT_BASE, %#lx\n", auxv[i].a_un.a_val);		// .a_ptr
				break;
			case AT_FLAGS:			// 8: Flags
				fprintf(stderr, "AT_FLAGS, %#lx\n", auxv[i].a_un.a_val);
				break;
			case AT_ENTRY:			// 9: Entry point of program
				fprintf(stderr, "AT_ENTRY, %#lx\n", auxv[i].a_un.a_val);	// .a_ptr
				break;
			case AT_NOTELF:			// 10: Program is not ELF
				fprintf(stderr, "AT_NOTELF, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_UID:			// 11: Real uid
				fprintf(stderr, "AT_UID, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_EUID:			// 12: Effective uid
				fprintf(stderr, "AT_EUID, %ld\n", auxv[i].a_un.a_val);
				break;	
			case AT_GID:			// 13: Real gid
				fprintf(stderr, "AT_GID, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_EGID:			// 14: Effective gid
				fprintf(stderr, "AT_EGID, %ld\n", auxv[i].a_un.a_val);
				break;
			case AT_CLKTCK:			// 17: Frequency of times()
				fprintf(stderr, "AT_CLKTCK, %ld\n", auxv[i].a_un.a_val);
				break;
			// Pointer to the global system page used for system calls and other nice things.
			case AT_SYSINFO:		// 22
				fprintf(stderr, "AT_SYSINFO, %#lx\n", auxv[i].a_un.a_val);
				break;
			case AT_SYSINFO_EHDR:	// 33
				fprintf(stderr, "AT_SYSINFO_EHDR, %#lx\n", auxv[i].a_un.a_val);
				break;
			default:				// ???
				fprintf(stderr, "AT_??? (%ld), %ld\n", auxv[i].a_type, auxv[i].a_un.a_val);
				break;
		}
		if (auxv[i].a_type == AT_NULL) {
			auxc = i;
			break;
		}
	}
	printf("auxc = %d\n", auxc);
}
 */


#endif