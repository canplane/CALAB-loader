#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>

#include <errno.h>



// adapted from linux/fs/binfmt_elf.c
#define PAGE_SIZE			(size_t)(1 << 12)	// 4096 B
#define PAGE_FLOOR(_addr)	((_addr) & ~(size_t)(PAGE_SIZE - 1))					// ELF_PAGESTART
#define PAGE_OFFSET(_addr)	((_addr) & (PAGE_SIZE - 1))								// ELF_PAGEOFFSET
#define PAGE_CEIL(_addr)	(((_addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))			// ELF_PAGEALIGN

#define STACK_SIZE			(size_t)(1 << 26)	// 8192 KB
#define STACK_LOW			0x10000000L
#define STACK_HIGH			(STACK_LOW + STACK_SIZE)



extern Elf64_Ehdr e_header;

Elf64_Addr create_stack(const char *argv[], const char *envp[])
{
	int i;

	/* allocate new stack space */

	fprintf(stderr, "Mapping: stack -> (memory address = %#lx, size = %#lx)\n", STACK_LOW, STACK_SIZE);
	if (mmap((void *)STACK_LOW, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
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

	sp = STACK_HIGH;										// bottom of stack

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
				new_auxv[i].a_un.a_val = e_header.e_phnum;
				//fprintf(stderr, "Auxiliary vector modified: AT_PHNUM: %ld -> %ld\n", auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
			case AT_BASE:			// 7: Base address of interpreter
				new_auxv[i].a_un.a_val = 0;
				//fprintf(stderr, "Auxiliary vector modified: AT_BASE: %#lx -> %#lx\n", auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
			case AT_ENTRY:			// 9: Entry point of program
				new_auxv[i].a_un.a_val = e_header.e_entry;
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

...8:	argv[0] ->	argv[0]	-> 	argv[0]	->	argv[0]	-> 	argv[0]
...0:	argc					[]			[]			[]
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



/* for debugging */

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