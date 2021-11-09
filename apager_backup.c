#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>

#include <errno.h>

extern int errno;


// adapted from linux/fs/binfmt_elf.c
#define PAGE_SIZE			(1 << 12)	// 4096 B
#define PAGE_FLOOR(_addr)	((_addr) & ~(unsigned long)(PAGE_SIZE - 1))			// ELF_PAGESTART
#define PAGE_OFFSET(_addr)	((_addr) & (PAGE_SIZE - 1))							// ELF_PAGEOFFSET
#define PAGE_CEIL(_addr)	(((_addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))		// ELF_PAGEALIGN

#define STACK_SIZE			(1 << 26)	// 8192 KB
#define STACK_LOW			0x10000000L
#define STACK_HIGH			(STACK_LOW + STACK_SIZE)


void print_elf_header(Elf64_Ehdr *ep)
{
	printf("ELF header {\n");
	printf("\tunsigned char \te_ident[16]: \t%s\n", ep->e_ident);		// ELF identification
    printf("\tElf64_Half \te_type: \t%#x\n", ep->e_type);				// Object file type
    printf("\tElf64_Half \te_machine: \t%#x\n", ep->e_machine);			// Machine type
    printf("\tElf64_Word \te_version: \t%u\n", ep->e_version);			// Object file version
    printf("\tElf64_Addr \te_entry: \t%#lx\n", ep->e_entry);			// Entry point address
	printf("\tElf64_Off \te_phoff: \t%#lx\n", ep->e_phoff);				// Program header offset
	printf("\tElf64_Off \te_shoff: \t%#lx\n", ep->e_shoff);				// Section header offset
    printf("\tElf64_Word \te_flags: \t%#x\n", ep->e_flags);				// Processor-specific flags
    printf("\tElf64_Half \te_ehsize: \t%u\n", ep->e_ehsize);			// ELF header size
    printf("\tElf64_Half \te_phentsize: \t%u\n", ep->e_phentsize);		// Size of program header entry
        // equal to sizeof(Elf64_Ehdr) = 64
	printf("\tElf64_Half \te_phnum: \t%u\n", ep->e_phnum);				// Number of program header entries
    printf("\tElf64_Half \te_shentsize: \t%u\n", ep->e_shentsize);		// Size of section header entry
    printf("\tElf64_Half \te_shnum: \t%u\n", ep->e_shnum);				// Number of section header entries
    printf("\tElf64_Half \te_shstrndx: \t%u\n", ep->e_shentsize);		// Section name string table index
	printf("}\n");
}
void print_program_header_entry(int i, Elf64_Phdr *pp)
{
	printf("Program header entry [%d] {\n", i);
    printf("\tElf64_Word \tp_type: \t%#x\n", pp->p_type);				// Type of segment
    printf("\tElf64_Word \tp_flags: \t%#x\n", pp->p_flags);				// Segment attributes
    printf("\tElf64_Off \tp_offset: \t%#lx\n", pp->p_offset);			// Offset in file
    printf("\tElf64_Addr \tp_vaddr: \t%#lx\n", pp->p_vaddr);			// Virtual address in memory
    	//printf("\tElf64_Addr \tp_paddr: \t%#lx\n", pp->p_paddr);			// Reserved
    printf("\tElf64_Xword \tp_filesz: \t%#lx\n", pp->p_filesz);			// Size of segment in file
    printf("\tElf64_Xword \tp_memsz: \t%#lx\n", pp->p_memsz);			// Size of segment in memory
    printf("\tElf64_Xword \tp_align: \t%#lx\n", pp->p_align);			// Alignment of segment
	printf("}\n");
}
void print_stack(Elf64_Addr sp)
{
	int i;

	char **argv, **envp;
	Elf64_auxv_t *auxv;
	int argc, envc, auxc;

	printf("argc = %ld\n", *(int64_t *)sp);					// argc
	sp += sizeof(int64_t);

	argv = (char **)sp;										// argv
	for (i = 0; argv[i]; i++) {
		printf("(%p) argv[%d] -> (%p) \"%s\"\n", &argv[i], i, argv[i], argv[i]);
		if (!argv[i])
			break;
	}
	argc = i;

	envp = (char **)(argv + argc + 1);						// envp
	for (i = 0; envp[i]; i++) {
		printf("(%p) envp[%d] -> (%p) \"%s\"\n", &envp[i], i, envp[i], envp[i]);
		if (!envp[i])
			break;
	}
	envc = i;

	auxv = (Elf64_auxv_t *)(envp + envc + 1);				// auxc
	for (i = 0; auxv[i].a_type != AT_NULL; i++)
		;
	auxc = i;
	printf("auxc = %d\n", auxc);
}


int elf_map(Elf64_Addr addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    // align to page
	// Loadable process segments must have congruent values for p_vaddr and p_offset, modulo the page size.
	addr = PAGE_FLOOR(addr), offset = PAGE_FLOOR(offset), len = PAGE_CEIL(PAGE_OFFSET(addr) + len);
	
	if (mmap((void *)addr, len, prot, flags, fd, offset) == MAP_FAILED) {
		fprintf(stderr, "Error: Mapping failed: offset [%#lx, %#lx) -> [%#lx, %#lx): %s\n", offset, offset + len, addr, addr + len, strerror(errno));
		exit(1);
	}
	fprintf(stderr, "Mapping: offset [%#lx, %#lx) -> [%#lx, %#lx) (size: %ld)\n", offset, offset + len, addr, addr + len, len);
    return 0;
}
// read-write zero-initialized anonymous memory
int elf_map_bss(Elf64_Addr start, Elf64_Addr end, int prot, int flags)
{
	Elf64_Addr prev_start = start;
	
    // align to page
	start = PAGE_CEIL(start), end = PAGE_CEIL(end);
	if (start == end)
		return 0;
	
	// zero-fill
	if (prev_start < start)
		memset((void *)prev_start, 0, start - prev_start);

	// MAP_ANONYMOUS: The mapping is not backed by any file; its contents are initialized to zero.
	if (mmap((void *)start, end - start, prot, flags | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		fprintf(stderr, "Error: Mapping failed: .bss -> [%#lx, %#lx): %s\n", start, end, strerror(errno));
		exit(1);
	}
	fprintf(stderr, "Mapping: .bss -> [%#lx, %#lx) (size: %ld)\n", start, end, end - start);
    return 0;
}
int load_elf_binary(const char *path, Elf64_Ehdr *elf_header_p, Elf64_Addr *load_addr)
{
	/* open the program */

	int fd;
	if ((fd = open(path, O_RDONLY)) == -1) {
        fprintf(stderr, "Error: Cannot open the program ’%s’: %s\n", path, strerror(errno));
		exit(1);
    }


	/* read ELF header */
	
	if (read(fd, elf_header_p, sizeof(Elf64_Ehdr)) == -1) {
		perror("Error: Cannot read ELF header");
		exit(1);
	}
    //print_elf_header(elf_header_p);		// debug

	if (strncmp((const char *)elf_header_p->e_ident, "\x7f""ELF", 4)) {		// magic number
		fprintf(stderr, "Error: Not ELF object file\n");
		exit(1);
	}
	if (elf_header_p->e_ident[EI_CLASS] != ELFCLASS64) {
		fprintf(stderr, "Error: Not 64-bit object\n");
		exit(1);
	}
    if (elf_header_p->e_type != ET_EXEC) {	// only support for ET_EXEC(static linked executable), not ET_DYN
		fprintf(stderr, "Error: This loader only support static linked executables (ET_EXEC)\n");
		exit(1);
	}
	

	/* read program header table */

	Elf64_Phdr program_header_entry;
	int elf_prot;
	int elf_flags = MAP_PRIVATE | MAP_FIXED;	// valid in ET_EXEC
	Elf64_Addr bss_start, bss_end;
	
	int load_addr_set = 0;
	*load_addr = 0;

	if (lseek(fd, elf_header_p->e_phoff, SEEK_SET) == -1) {
		perror("Error: lseek()");
		exit(1);
	}
	for (int i = 0; i < elf_header_p->e_phnum; i++) {
		if (read(fd, &program_header_entry, sizeof(Elf64_Phdr)) == -1) {
			perror("Error: Cannot read a program header entry");
			exit(1);
		}

		if (program_header_entry.p_type != PT_LOAD)
            continue;
		//puts(""), print_program_header_entry(i, &program_header_entry);	// debug
		
		elf_prot = 0;
		if (program_header_entry.p_flags & PF_R)	elf_prot |= PROT_READ;
		if (program_header_entry.p_flags & PF_W)	elf_prot |= PROT_WRITE;
		if (program_header_entry.p_flags & PF_X)	elf_prot |= PROT_EXEC;

		elf_map(program_header_entry.p_vaddr, program_header_entry.p_filesz, elf_prot, elf_flags, fd, program_header_entry.p_offset);
		
		if (!load_addr_set) {
			load_addr_set = 1;
			*load_addr = program_header_entry.p_vaddr - program_header_entry.p_offset;
		}

		// .bss
        bss_start = program_header_entry.p_vaddr + program_header_entry.p_filesz;
        bss_end = program_header_entry.p_vaddr + program_header_entry.p_memsz;
        if (bss_start < bss_end)	// !(bss_start == bss_end)
			elf_map_bss(bss_start, bss_end, elf_prot, elf_flags);
	}

	close(fd);
	return 0;
}


Elf64_Addr create_elf_tables(char *argv[], char *envp[], Elf64_Ehdr *elf_header_p, Elf64_Addr load_addr)
{
	int i;


	/* get additional memory layout information */

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


	/* allocate new stack space -> [STACK_LOW, STACK_LOW + STACK_SIZE) */

	Elf64_Addr sp;

	if (mmap((void *)STACK_LOW, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		fprintf(stderr, "Error: Mapping failed: stack -> [%#lx, %#lx): %s\n", STACK_LOW, STACK_LOW + STACK_SIZE, strerror(errno));
		exit(1);
	}
	fprintf(stderr, "Mapping: stack -> [%#lx, %#lx) (size: %ld)\n", STACK_LOW, STACK_LOW + STACK_SIZE, (size_t)STACK_SIZE);


	/* init stack pointer and resolve new addresses by information */
	
	char **new_argv, **new_envp;
	Elf64_auxv_t *new_auxv;
	char *new_argv_asciiz_space, *new_envp_asciiz_space;

	sp = STACK_LOW + STACK_SIZE;  							// bottom of stack

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
		switch (new_auxv[i].a_type) {
			case AT_PHDR:	// Program headers for program
				//printf("AT_PHDR: %#lx -> %#lx\n", new_auxv[i].a_un.a_val, load_addr + elf_header_p->e_phoff);
				new_auxv[i].a_un.a_val = load_addr + elf_header_p->e_phoff;
				break;
			case AT_PHNUM:	// Number of program headers
				//printf("AT_PHNUM: %lu -> %u\n", new_auxv[i].a_un.a_val, elf_header_p->e_phnum);
				new_auxv[i].a_un.a_val = elf_header_p->e_phnum;
				break;
			case AT_ENTRY:	// Entry point of program
				//printf("AT_ENTRY: %#lx -> %#lx\n", new_auxv[i].a_un.a_val, elf_header_p->e_entry);
				new_auxv[i].a_un.a_val = elf_header_p->e_entry;
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


void start_thread(Elf64_Addr entry, Elf64_Addr sp)
{
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
	__asm__ __volatile__ (
		"movq $0, %%rdx\n\t"
		"movq %0, %%rsp\n\t"
		"jmp *%1"
		: : "a" (sp), "b" (entry)
	);
}


int my_execve(const char *path, char *argv[], char *envp[])
{
	Elf64_Ehdr elf_header;
	Elf64_Addr sp;

	Elf64_Addr load_addr;
	load_elf_binary(path, &elf_header, &load_addr);
	sp = create_elf_tables(argv, envp, &elf_header, load_addr);
	//print_stack(sp);	// debug

	puts("");
	printf("Executing the program ’%s’... (Stack pointer = %#lx, Entry address = %#lx)\n", path, sp, elf_header.e_entry);
	printf("--------\n");
	
	start_thread(elf_header.e_entry, sp);	// context switch

	return -1;
}
#define execve my_execve


int main(int argc, char *argv[], char *envp[])
{
	char **new_argv;

	if (argc < 2) {
        fprintf(stderr, "Usage: %s file [args ...]\n", argv[0]);
        exit(1);
    }
	new_argv = &argv[1];

	execve(new_argv[0], new_argv, envp);

	printf("This is never printed\n");
	return 0;
}