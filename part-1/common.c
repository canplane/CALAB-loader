/* 
 * /part-1/common.c
 * ----------------
 * CALAB Master Programming Project - (Part 1) User-level loader
 * 
 * Sanghoon Lee (canplane@gmail.com)
 * 12 November 2021
 */


#ifndef			__COMMON_C__
#define			__COMMON_C__




#include 		<stdio.h>
#include 		<stdlib.h>
#include 		<string.h>

#include 		<fcntl.h>
#include 		<unistd.h>
#include		<sys/mman.h>
#include 		<elf.h>

#include 		<errno.h>

extern int 		errno;


//#include		"../etc/debug.c"




/* font style */

#define			ERR_STYLE__								"\x1b[2m\x1b[3m"	// bold, italic
#define			UND_STYLE__								"\x1b[4m"			// underline
#define			INV_STYLE__								"\x1b[7m"			// inverse
#define			__ERR_STYLE								"\x1b[0m"			// reset to normal


#define			__ERR_STYLE								"\x1b[0m"	




/* memory layout */

// adapted from linux/fs/binfmt_elf.c
#define 		PAGE_SIZE								(unsigned long)(1 << 12)	// 4 KB
#define 		PAGE_FLOOR(_addr)						((_addr) & ~(unsigned long)(PAGE_SIZE - 1))			// ELF_PAGESTART
#define 		PAGE_OFFSET(_addr)						((_addr) & (PAGE_SIZE - 1))							// ELF_PAGEOFFSET
#define 		PAGE_CEIL(_addr)						(((_addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))		// ELF_PAGEALIGN


#define			THREAD_SPACE_LOW						0L
#define			THREAD_SPACE_HIGH						0x50000000L


#define			STACK_SIZE								(unsigned long)(1 << 23)	// 8 MB

#define			STACK_SPACE_LOW							(THREAD_SPACE_HIGH - STACK_SIZE)
#define			STACK_SPACE_HIGH						THREAD_SPACE_HIGH

#define			SEGMENT_SPACE_LOW						THREAD_SPACE_LOW
#define			SEGMENT_SPACE_HIGH						STACK_SPACE_LOW


#define			BASE_ADDR								0x60000000L		// gcc -Ttext-segment=(BASE_ADDR) ...


#define			P_HEADER_TABLE_LOW						THREAD_SPACE_HIGH
#define			P_HEADER_TABLE_HIGH						BASE_ADDR
#define			P_HEADER_TABLE_MAXSZ					(P_HEADER_TABLE_HIGH - P_HEADER_TABLE_LOW)




/* thread */

int				fd;
Elf64_Phdr		*p_header_table;
int				p_header_num;




/* read information of segments from ELF binary file */

Elf64_Ehdr read_elf_binary(const char *path)
{
	/* open the program */

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
	
	if (e_header.e_phnum * sizeof(Elf64_Phdr) > P_HEADER_TABLE_MAXSZ) {
		fprintf(stderr, "Error: The size of program header table is too large to store into memory. Cannot exceed %#lx.\n", P_HEADER_TABLE_MAXSZ);
		exit(1);
	}
	fprintf(stderr, ERR_STYLE__"Mapping: Program header table -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, P_HEADER_TABLE_LOW, PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)));
	if (mmap((void *)P_HEADER_TABLE_LOW, PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Cannot allocate program header table into memory");
		exit(1);
	}
	p_header_table = (Elf64_Phdr *)P_HEADER_TABLE_LOW, p_header_num = e_header.e_phnum;
	

	/* read program header table */

	if (lseek(fd, e_header.e_phoff, SEEK_SET) == -1) {
		perror("Error: lseek failed");
		exit(1);
	}
	if (read(fd, p_header_table, e_header.e_phnum * sizeof(Elf64_Phdr)) == -1) {
		perror("Error: Cannot read a program header table");
		exit(1);
	}
	if (mprotect((void *)p_header_table, PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)), PROT_READ) == -1) {
		perror("Error: mprotect failed");
		exit(1);
	}


	/* check address range of segments */

	for (int i = 0; i < e_header.e_phnum; i++) {
		//fprintf(stderr, "Program header entry %d ", i), print_p_header(&p_header_table[i]);
		if (p_header_table[i].p_type != PT_LOAD)
            continue;

		if (!((SEGMENT_SPACE_LOW <= p_header_table[i].p_vaddr) && (p_header_table[i].p_vaddr + p_header_table[i].p_memsz <= SEGMENT_SPACE_HIGH))) {
			fprintf(stderr, "Error: Cannot support address range used by the program. This loader only supports for the range from %#lx to %#lx. Try gcc option '-Ttext-segment=%#lx'.\n", SEGMENT_SPACE_LOW, SEGMENT_SPACE_HIGH, SEGMENT_SPACE_LOW);
			exit(1);
		}
	}

	return e_header;
}




Elf64_Addr create_stack(const char *argv[], const char *envp[], const Elf64_Ehdr *ep)
{
	int i;

	/* allocate new stack space */

	fprintf(stderr, ERR_STYLE__"Mapping: Stack -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, STACK_SPACE_LOW, STACK_SIZE);
	if (mmap((void *)STACK_SPACE_LOW, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Memory mapping failed");
		exit(1);
	}


	/* get memory layout information */

	Elf64_auxv_t *auxv;
	int argc, envc, auxc;
	size_t argv_asciiz_space_size, envp_asciiz_space_size;

	argv_asciiz_space_size = 0;								// argc, argument ASCIIZ strings
	for (i = 0; argv[i]; i++)
		argv_asciiz_space_size += strlen(argv[i]) + 1;
	argc = i;

	envp_asciiz_space_size = 0;								// envc, environment ASCIIZ strings
	for (i = 0; envp[i]; i++)
		envp_asciiz_space_size += strlen(envp[i]) + 1;
	envc = i;

	auxv = (Elf64_auxv_t *)(envp + envc + 1);				// auxc
	for (i = 0; auxv[i].a_type != AT_NULL; i++)
		;
	auxc = i;


	/* init stack pointer and resolve new addresses by information */

	Elf64_Addr sp;
	
	char **new_argv, **new_envp;
	Elf64_auxv_t *new_auxv;
	char *new_argv_asciiz_space, *new_envp_asciiz_space;

	sp = STACK_SPACE_HIGH;									// bottom of stack

	sp -= sizeof(int64_t);									// end marker

	sp -= envp_asciiz_space_size;							// environment ASCIIZ string space
	new_envp_asciiz_space = (char *)sp;
	
	sp -= argv_asciiz_space_size;							// argument ASCIIZ string space
	new_argv_asciiz_space = (char *)sp;
	
	sp = sp & ~0xf;											// padding (0~16)
	
	sp -= (auxc + 1) * sizeof(Elf64_auxv_t);				// auxv
	new_auxv = (Elf64_auxv_t *)sp;
	
	sp -= (envc + 1) * sizeof(char *);						// envp
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
				//fprintf(stderr, "Auxiliary vector modified: AT_PHNUM: %ld -> %ld\n", auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
			case AT_BASE:			// 7: Base address of interpreter
				new_auxv[i].a_un.a_val = 0;
				//fprintf(stderr, "Auxiliary vector modified: AT_BASE: %#lx -> %#lx\n", auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
			case AT_ENTRY:			// 9: Entry point of program
				new_auxv[i].a_un.a_val = ep->e_entry;
				//fprintf(stderr, "Auxiliary vector modified: AT_ENTRY: %#lx -> %#lx\n", auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
		}
	}

	_sp = (Elf64_Addr)new_envp_asciiz_space;				// envp, environment ASCIIZ strings
	for (i = 0; i < envc; i++) {
		memcpy((void *)_sp, envp[i], len = strlen(envp[i]) + 1);
		new_envp[i] = (void *)_sp, _sp += len;
	}

	_sp = (Elf64_Addr)new_argv_asciiz_space;				// argv, argument ASCIIZ strings
	for (i = 0; i < argc; i++) {
		memcpy((void *)_sp, argv[i], len = strlen(argv[i]) + 1);
		new_argv[i] = (void *)_sp, _sp += len;
	}

	*(int64_t *)sp = (int64_t)argc;							// argc


	/* return stack pointer */

	//print_stack((const char **)(sp + sizeof(unsigned long)));
	return sp;
}



void start(Elf64_Addr entry, Elf64_Addr sp)
{
	__asm__ __volatile__ (
		"movq $0, %%rdx\n\t"
		"movq %0, %%rsp\n\t"
		"jmp *%1"
		: : "a" (sp), "b" (entry)
	);

    /*
		<_start>
		
		argv[0] ->	argv[0]	-> 	argv[0]	->	argv[0]	-> 	argv[0]
		argc					[]			[]			[]
											%rax		%rax
														&(%rax)
		// %r9 = %rdx
					// %rsi = argc
					// %rdx = (%rsp = &argv[0])
														// %r8  = _libc_csu_fini
														// %rcx = _libc_csu_init
														// %rdi = main

		_libc_start_main(
			%rdi=main, %rsi=argc, %rdx=argv, %rcx=_libc_csu_init, %r8=_libc_csu_fini, %r9
		)
	 */
}




#endif