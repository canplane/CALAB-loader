#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>

#include <errno.h>

extern int errno;

#define MIN2(a, b)			((a) > (b) ? (b) : (a))

// adapted from linux/fs/binfmt_elf.c
#define PAGE_SIZE			(1 << 12)	// 4096 B
#define PAGE_FLOOR(_addr)	((_addr) & ~(unsigned long)(PAGE_SIZE - 1))			// ELF_PAGESTART
#define PAGE_OFFSET(_addr)	((_addr) & (PAGE_SIZE - 1))							// ELF_PAGEOFFSET
#define PAGE_CEIL(_addr)	(((_addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))		// ELF_PAGEALIGN

#define STACK_SIZE			(1 << 26)	// 8192 KB
#define STACK_LOW			0x10000000L
#define STACK_HIGH			(STACK_LOW + STACK_SIZE)


/*
void print_e_header(const Elf64_Ehdr *e)
{
	fprintf(stderr, "ELF header {\n");
	fprintf(stderr, "\tunsigned char \te_ident[16] \t= \"%s\"\n", e->e_ident);	// ELF identification
    fprintf(stderr, "\tElf64_Half \te_type \t\t= %#x\n", e->e_type);			// Object file type
    fprintf(stderr, "\tElf64_Half \te_machine \t= %#x\n", e->e_machine);		// Machine type
    fprintf(stderr, "\tElf64_Word \te_version \t= %u\n", e->e_version);			// Object file version
    fprintf(stderr, "\tElf64_Addr \te_entry \t= %#lx\n", e->e_entry);			// Entry point address
	fprintf(stderr, "\tElf64_Off \te_phoff \t= %#lx\n", e->e_phoff);			// Program header offset
	fprintf(stderr, "\tElf64_Off \te_shoff \t= %#lx\n", e->e_shoff);			// Section header offset
    fprintf(stderr, "\tElf64_Word \te_flags \t= %#x\n", e->e_flags);			// Processor-specific flags
    fprintf(stderr, "\tElf64_Half \te_ehsize \t= %u\n", e->e_ehsize);			// ELF header size (equal to sizeof(Elf64_Ehdr))
    fprintf(stderr, "\tElf64_Half \te_phentsize \t= %u\n", e->e_phentsize);		// Size of program header entry
	fprintf(stderr, "\tElf64_Half \te_phnum \t= %u\n", e->e_phnum);				// Number of program header entries
    fprintf(stderr, "\tElf64_Half \te_shentsize \t= %u\n", e->e_shentsize);		// Size of section header entry
    fprintf(stderr, "\tElf64_Half \te_shnum \t= %u\n", e->e_shnum);				// Number of section header entries
    fprintf(stderr, "\tElf64_Half \te_shstrndx \t= %u\n", e->e_shentsize);		// Section name string table index
	fprintf(stderr, "}\n");
}
void print_p_header(int idx, const Elf64_Phdr *p)
{
	fprintf(stderr, "Program header entry [%d] {\n", idx);
    fprintf(stderr, "\tElf64_Word \tp_type \t\t= %#x\n", p->p_type);			// Type of segment
    fprintf(stderr, "\tElf64_Word \tp_flags \t= %#x\n", p->p_flags);			// Segment attributes
    fprintf(stderr, "\tElf64_Off \tp_offset \t= %#lx\n", p->p_offset);			// Offset in file
    fprintf(stderr, "\tElf64_Addr \tp_vaddr \t= %#lx\n", p->p_vaddr);			// Virtual address in memory
    //fprintf(stderr, "\tElf64_Addr \tp_paddr \t= %#lx\n", p->p_paddr);			// Reserved
    fprintf(stderr, "\tElf64_Xword \tp_filesz \t= %#lx\n", p->p_filesz);		// Size of segment in file
    fprintf(stderr, "\tElf64_Xword \tp_memsz \t= %#lx\n", p->p_memsz);			// Size of segment in memory
    fprintf(stderr, "\tElf64_Xword \tp_align \t= %#lx\n", p->p_align);			// Alignment of segment
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


int fd;
Elf64_Ehdr e_header;
Elf64_Phdr *p_headers;
Elf64_Addr load_addr;


void *elf_map(Elf64_Addr addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	// Loadable process segments must have congruent values for p_vaddr and p_offset, modulo the page size.
	addr = PAGE_FLOOR(addr), offset = PAGE_FLOOR(offset), len = PAGE_CEIL(PAGE_OFFSET(addr) + len);
	
	fprintf(stderr, "Mapping: offset [%#lx, %#lx) -> [%#lx, %#lx) (size = %#lx)\n", offset, offset + len, addr, addr + len, len);
	return mmap((void *)addr, len, prot, flags, fd, offset);
}
// read-write zero-initialized anonymous memory
void *elf_map_bss(Elf64_Addr addr, size_t len, int prot, int flags)
{
	Elf64_Addr _addr = addr;
	addr = PAGE_CEIL(_addr), len = PAGE_CEIL(_addr + len) - PAGE_CEIL(_addr);

	if (_addr < addr)
		memset((void *)_addr, 0, addr - _addr);		// zero-fill
	if (!len)
		return NULL;
	
	fprintf(stderr, "Mapping: .bss -> [%#lx, %#lx) (size = %#lx)\n", addr, addr + len, len);
	return mmap((void *)addr, len, prot, flags | MAP_ANONYMOUS, -1, 0);
}

void map_segment(int idx) {
	int elf_prot = 0;
	int elf_flags = MAP_PRIVATE | MAP_FIXED;	// valid in ET_EXEC

	if (p_headers[idx].p_flags & PF_R)	elf_prot |= PROT_READ;
	if (p_headers[idx].p_flags & PF_W)	elf_prot |= PROT_WRITE;
	if (p_headers[idx].p_flags & PF_X)	elf_prot |= PROT_EXEC;

	if (elf_map(p_headers[idx].p_vaddr, p_headers[idx].p_filesz, elf_prot, elf_flags, fd, p_headers[idx].p_offset) == MAP_FAILED) {
		perror("Error: Memory mapping failed");
		exit(1);
	}
	
	// .bss
	if (p_headers[idx].p_filesz < p_headers[idx].p_memsz) {
		if (elf_map_bss(p_headers[idx].p_vaddr + p_headers[idx].p_filesz, p_headers[idx].p_memsz - p_headers[idx].p_filesz, elf_prot, elf_flags) == MAP_FAILED) {
			perror("Error: Memory mapping failed");
			exit(1);
		}
	}
}


void load_elf_binary(const char *path)
{
	/* open the program */

	if ((fd = open(path, O_RDONLY)) == -1) {
        fprintf(stderr, "Error: Cannot open the program '%s': %s\n", path, strerror(errno));
		exit(1);
    }


	/* read ELF header */
	
	if (read(fd, &e_header, sizeof(Elf64_Ehdr)) == -1) {
		perror("Error: Cannot read ELF header");
		exit(1);
	}
    //print_e_header(&e_header);

	if (strncmp((const char *)e_header.e_ident, "\x7f""ELF", 4)) {		// magic number
		fprintf(stderr, "Error: Not ELF object file\n");
		exit(1);
	}
	if (e_header.e_ident[EI_CLASS] != ELFCLASS64) {
		fprintf(stderr, "Error: Not 64-bit object\n");
		exit(1);
	}
    if (e_header.e_type != ET_EXEC) {	// only support for ET_EXEC(static linked executable), not ET_DYN
		fprintf(stderr, "Error: This loader only support static linked executables (ET_EXEC)\n");
		exit(1);
	}
	

	/* read program header table */

	if ((p_headers = malloc(e_header.e_phnum * sizeof(Elf64_Phdr))) == NULL) {
		perror("Error: Cannot allocate memory for a program header table");
		exit(1);
	}
	if (lseek(fd, e_header.e_phoff, SEEK_SET) == -1) {
		perror("Error: lseek() failed");
		exit(1);
	}
	if (read(fd, p_headers, e_header.e_phnum * sizeof(Elf64_Phdr)) == -1) {
		perror("Error: Cannot read a program header table");
		exit(1);
	}

	
	/* calculate load address and map into memory */

	load_addr = (Elf64_Addr)-1;
	for (int i = 0; i < e_header.e_phnum; i++) {
		//print_p_header(i, &p_headers[i]);

		if (p_headers[i].p_type != PT_LOAD)
            continue;
		load_addr = MIN2(load_addr, p_headers[i].p_vaddr - p_headers[i].p_offset);
		
		map_segment(i);
	}

	close(fd);
}


Elf64_Addr create_elf_tables(const char *argv[], const char *envp[])
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

	fprintf(stderr, "Mapping: stack -> [%#lx, %#lx) (size = %#lx)\n", STACK_LOW, STACK_LOW + STACK_SIZE, (size_t)STACK_SIZE);
	if (mmap((void *)STACK_LOW, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Memory mapping failed");
		exit(1);
	}

	/* init stack pointer and resolve new addresses by information */
	
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
			case AT_EXECFD:			// 2: File descriptor of program
				auxv[i].a_type = AT_IGNORE;
				//fprintf(stderr, "Auxiliary vector modified: AT_EXECFD: %ld -> ignored\n", auxv[i].a_un.a_val);
				break;
			case AT_PHDR:			// 3: Program headers for program
				new_auxv[i].a_un.a_val = load_addr + e_header.e_phoff;
				//fprintf(stderr, "Auxiliary vector modified: AT_PHDR: %#lx -> %#lx\n", auxv[i].a_un.a_val, new_auxv[i].a_un.a_val);
				break;
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
int my_execve(const char *path, const char *argv[], const char *envp[])
{
	Elf64_Addr sp;

	load_elf_binary(path);
	sp = create_elf_tables(argv, envp);
	//print_stack((const char **)(sp + sizeof(int64_t)));

	fprintf(stderr, "Executing the program '%s'... (Stack pointer = %#lx, Entry address = %#lx)\n", path, sp, e_header.e_entry);
	fprintf(stderr, "--------\n");
	
	start_thread(e_header.e_entry, sp);	// context switch

	return -1;
}
#define execve 				my_execve


int main(int argc, const char **argv, const char **envp)
{
	if (argc < 2) {
        fprintf(stderr, "Usage: %s file [args ...]\n", argv[0]);
        exit(1);
    }

	execve(argv[1], argv + 1, envp);

	printf("This is never printed\n");
	return 0;
}