/* 
 * /part-2/3-thread/common.c
 * ----------------
 * CALAB Master Programming Project - (Part 2) Advanced user-level loader
 * User-level threading
 * 
 * Sanghoon Lee (canplane@gmail.com)
 * 12 November 2021
 */


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

#include		"../../etc/debug.c"




/* font style */

#define			ERR_STYLE__								"\x1b[2m\x1b[3m"	// bold, italic
#define			INV_STYLE__								"\x1b[7m"			// inverse
#define			UND_STYLE__								"\x1b[4m"			// underline
#define			__ERR_STYLE								"\x1b[0m"			// reset to normal




/* memory layout */

// adapted from linux/fs/binfmt_elf.c
#define			_LOG2_PAGE_SIZE							12		// 4 KB
#define 		PAGE_SIZE								(unsigned long)(1 << _LOG2_PAGE_SIZE)
#define 		PAGE_FLOOR(_addr)						((_addr) & ~(unsigned long)(PAGE_SIZE - 1))			// ELF_PAGESTART
#define 		PAGE_OFFSET(_addr)						((_addr) & (PAGE_SIZE - 1))							// ELF_PAGEOFFSET
#define 		PAGE_CEIL(_addr)						(((_addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))		// ELF_PAGEALIGN


#define			_LOG2_THREAD_MAX_NUM					3		// this loader can load at most 8 programs
#define			THREAD_MAX_NUM							(1 << _LOG2_THREAD_MAX_NUM)


#define			THREADS_LOW								0x10000000L
#define			THREADS_HIGH							0x50000000L
#define			_THREADS_STRIDE							((THREADS_HIGH - THREADS_LOW) >> _LOG2_THREAD_MAX_NUM)
#define			THREAD_SPACE_LOW(_tid)					(THREADS_LOW + (_tid) * _THREADS_STRIDE)
#define			THREAD_SPACE_HIGH(_tid)					(THREADS_LOW + ((_tid) + 1) * _THREADS_STRIDE)


#define			_STACK_SIZE_WITH_PADDING				(unsigned long)(1 << 23)	// 8 MB
#define			PADDING									PAGE_SIZE

#define			STACK_SPACE_HIGH(_tid)					(THREAD_SPACE_HIGH(_tid) - PADDING)
#define			STACK_SPACE_LOW(_tid)					((THREAD_SPACE_HIGH(_tid) - _STACK_SIZE_WITH_PADDING) + PADDING)
#define			STACK_SIZE								(_STACK_SIZE_WITH_PADDING - (PADDING << 1))		// 8 MB - 8 KB

#define			SEGMENT_SPACE_LOW(_tid)					THREAD_SPACE_LOW(_tid)
#define			SEGMENT_SPACE_HIGH(_tid)				(THREAD_SPACE_HIGH(_tid) - _STACK_SIZE_WITH_PADDING)


#define			P_HEADER_TABLES_LOW						THREADS_HIGH
#define			P_HEADER_TABLE_MAXSZ					PAGE_SIZE
#define			P_HEADER_TABLES_HIGH					(P_HEADER_TABLES_LOW + (P_HEADER_TABLE_MAXSZ << _LOG2_THREAD_MAX_NUM))
#define			P_HEADER_TABLE(_tid)					(P_HEADER_TABLES_LOW + (_tid) * P_HEADER_TABLE_MAXSZ)


#define			BASE_ADDR								0x60000000L		// gcc -Ttext-segment=(BASE_ADDR) ...




/* thread */

#include		<stdarg.h>
#include		<setjmp.h>

#define			THREAD_STATE_NEW						0
#define			THREAD_STATE_RUN						1
#define			THREAD_STATE_WAIT						2
#define			THREAD_STATE_EXIT						3


typedef struct {
	int				fd;
	Elf64_Phdr		*p_header_table;
	int				p_header_num;

	int				state;
	jmp_buf			jmpenv;

	Elf64_Addr 		entry, sp;		// used at STATE_NEW
	int				exit_code;		// used at STATE_EXIT
} Thread;
Thread 			thread[THREAD_MAX_NUM];
int 			current_thread_idx = -1;

jmp_buf			loader_jmpenv;


// call by loader
int dispatch(int thread_id)
{
	current_thread_idx = thread_id;

	int state = thread[thread_id].state;
	thread[thread_id].state = THREAD_STATE_RUN;

	if (!setjmp(loader_jmpenv)) {		// context switch
		switch (state) {
			case THREAD_STATE_NEW:
				__asm__ __volatile__ (
					"movq $0, %%rdx\n\t"		// because an instruction (%r9 = %rdx) will be executed at subroutine <_start>
					"movq %0, %%rsp\n\t"
					"jmp *%1"
					: : "a" (thread[thread_id].sp), "b" (thread[thread_id].entry)
				);
				break;

			case THREAD_STATE_WAIT:
				longjmp(thread[thread_id].jmpenv, true);
				break;

			case THREAD_STATE_RUN:
			case THREAD_STATE_EXIT:
			default:
				fprintf(stderr, "Error: Cannot dispatch thread %d which state set to invalid value: %d\n", thread_id, thread[thread_id].state);
				exit(1);
				break;
		}			
	}

	current_thread_idx = -1;
	return 0;
}

void unmap_thread(int thread_id)
{
	Thread *th = &thread[thread_id];

	// free .text, .data, .bss
	Elf64_Addr segment_start, bss_end;
	for (int i = 0; i < th->p_header_num; i++) {
		if (th->p_header_table[i].p_type != PT_LOAD)
            continue;

		segment_start = PAGE_FLOOR(th->p_header_table[i].p_vaddr);
		bss_end = PAGE_CEIL(th->p_header_table[i].p_vaddr + th->p_header_table[i].p_memsz);

		fprintf(stderr, ERR_STYLE__"Thread %d: Unmapping: Segments (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, segment_start, bss_end - segment_start);
		if (munmap((void *)segment_start, bss_end - segment_start) == -1) {
			perror("Error: Cannot unmap memory for segments");
			exit(1);
		}
	}

	// free stack
	fprintf(stderr, ERR_STYLE__"Thread %d: Unmapping: Stack (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, STACK_SPACE_LOW(thread_id), STACK_SIZE);
	if (munmap((void *)STACK_SPACE_LOW(thread_id), STACK_SIZE) == -1) {
		perror("Error: Cannot unmap memory for stack");
		exit(1);
	}

	// free page header table
	fprintf(stderr, ERR_STYLE__"Thread %d: Unmapping: Program header table (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, P_HEADER_TABLE(thread_id), PAGE_CEIL(th->p_header_num * sizeof(Elf64_Phdr)));
	if (munmap((void *)P_HEADER_TABLE(thread_id), PAGE_CEIL(th->p_header_num * sizeof(Elf64_Phdr))) == -1) {
		perror("Error: Cannot unmap memory for program header table");
		exit(1);
	}
	
	// close file
	if (close(th->fd) == -1) {
		perror("Error: Cannot close file");
		exit(1);
	}
}




/* loader call */

#define			CALAB_LOADER__ENVVARNAME__CALL			"__CALAB_LOADER__CALL"

#define			CALAB_LOADER__CALL__exit				1
#define			CALAB_LOADER__CALL__yield				2


// interrupt service routine: call by running thread
int loader_ISR(int code, ...)
{
	va_list ap;
	va_start(ap, code);
	
	int ret = 0;
	switch (code) {
		case CALAB_LOADER__CALL__exit:  	// return_to_loader
			thread[current_thread_idx].state = THREAD_STATE_EXIT;
			ret = thread[current_thread_idx].exit_code = va_arg(ap, int);

			if (!setjmp(thread[current_thread_idx].jmpenv))
				longjmp(loader_jmpenv, true);
			break;

		case CALAB_LOADER__CALL__yield:		// yield
			thread[current_thread_idx].state = THREAD_STATE_WAIT;

			if (!setjmp(thread[current_thread_idx].jmpenv))
				longjmp(loader_jmpenv, true);
			break;

		default:
			fprintf(stderr, INV_STYLE__ ERR_STYLE__" Warning: Invalid call code: %d \n"__ERR_STYLE, code);
			ret = -1;
			break;
	}

	va_end(ap);
	return ret;
}




/* read information of segments from ELF binary file */

Elf64_Ehdr read_elf_binary(int thread_id, const char *path)
{
	/* open the program */

	int fd;
	if ((fd = open(path, O_RDONLY)) == -1) {
		fprintf(stderr, "Error: Cannot open file '%s': %s\n", path, strerror(errno));
		exit(1);
	}


	/* read ELF header */
	
	Elf64_Ehdr e_header;
	if (read(fd, &e_header, sizeof(Elf64_Ehdr)) == -1) {
		perror("Error: Cannot read ELF header");
		exit(1);
	}
	//fprintf(stderr, "ELF header "), print_e_header(&e_header);
	if (strncmp((const char *)e_header.e_ident, "\x7f""ELF", 4)) {		// magic number
		fprintf(stderr, "Error: Not ELF object file\n");
		exit(1);
	}
	if (e_header.e_ident[EI_CLASS] != ELFCLASS64) {
		fprintf(stderr, "Error: Not 64-bit object\n");
		exit(1);
	}
	if (e_header.e_type != ET_EXEC) {	// only support for ET_EXEC(static linked executable), not ET_DYN
		fprintf(stderr, "Error: This loader only support static linked executables. (ET_EXEC)\n");
		exit(1);
	}


	/* allocate memory to program header table */
	
	Elf64_Phdr *p_header_table;
	if (e_header.e_phnum * sizeof(Elf64_Phdr) > P_HEADER_TABLE_MAXSZ) {
		fprintf(stderr, "Error: The size of program header table is too large to store into memory. Cannot exceed %#lx.\n", P_HEADER_TABLE_MAXSZ);
		exit(1);
	}
	fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Program header table -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, P_HEADER_TABLE(thread_id), PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)));
	if (mmap((void *)P_HEADER_TABLE(thread_id), PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Cannot allocate program header table into memory");
		exit(1);
	}
	p_header_table = (Elf64_Phdr *)P_HEADER_TABLE(thread_id);
	

	/* read program header table */

	if (lseek(fd, e_header.e_phoff, SEEK_SET) == -1) {
		perror("Error: lseek failed");
		exit(1);
	}
	if (read(fd, p_header_table, e_header.e_phnum * sizeof(Elf64_Phdr)) == -1) {
		perror("Error: Cannot read a program header table");
		exit(1);
	}

	
	/* check address range of segments */

	for (int i = 0; i < e_header.e_phnum; i++) {
		//fprintf(stderr, "Program header entry %d ", i), print_p_header(&p_header_table[i]);
		if (p_header_table[i].p_type != PT_LOAD)
            continue;

		if (!((SEGMENT_SPACE_LOW(thread_id) <= p_header_table[i].p_vaddr) && (p_header_table[i].p_vaddr + p_header_table[i].p_memsz <= SEGMENT_SPACE_HIGH(thread_id)))) {
			fprintf(stderr, "Error: Cannot support address range used by the program. This loader only supports for the range from %#lx to %#lx. Try gcc option '-Ttext-segment=%#lx'.\n", SEGMENT_SPACE_LOW(thread_id), SEGMENT_SPACE_HIGH(thread_id), SEGMENT_SPACE_LOW(thread_id));
			exit(1);
		}
	}


	thread[thread_id].fd = fd;
	thread[thread_id].p_header_table = p_header_table, thread[thread_id].p_header_num = e_header.e_phnum;
	
	return e_header;
}




/* stack */

void make_additional_envp(char *envp_added[], char envp_added_asciiz_space[])
{
	int i;

	i = 0;
	i += sprintf(envp_added[0] = (envp_added_asciiz_space + 1), "%s=%p", CALAB_LOADER__ENVVARNAME__CALL, (void *)loader_ISR);
	envp_added[1] = NULL;
}

Elf64_Addr create_stack(int thread_id, const char *argv[], const char *envp[], const char *envp_added[], const Elf64_Ehdr *ep)
{
	int i;


	/* allocate new stack space */

	fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Stack -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, STACK_SPACE_LOW(thread_id), STACK_SIZE);
	if (mmap((void *)STACK_SPACE_LOW(thread_id), STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Memory mapping failed");
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

	//print_stack((const char **)(sp + sizeof(int64_t)));
	return sp;
}




#endif