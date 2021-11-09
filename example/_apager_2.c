// cc -static test.c -o test.out  // disable dynamic linking. ET_DYN -> ET_EXEC

// gcc -static test.c -g -o test.out

// gcc -S test.c -> test.s
// gcc -c test.c -> test.o
// od -A x -t x1 test.o
// readelf [-h | -l] test.o
// objdump -d test.o
// gdb -args apager.out test.out

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include <elf.h>

#define MAX2(a, b) 			((a) < (b) ? (b) : (a))

/* Adapted from linux/fs/binfmt_elf.c */
#define PAGE_SIZE			(1 << 12)	// 4096 B

#define ELF_PAGESTART(_v)	((_v) & ~(unsigned long)(PAGE_SIZE - 1))		// floor
#define ELF_PAGEOFFSET(_v)	((_v) & (PAGE_SIZE - 1))						// offset
#define ELF_PAGEALIGN(_v)	(((_v) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))		// ceil

#define STACK_SIZE			(1 << 26)	// 8192 KB
#define STACK_LOW			0x10000000
#define STACK_HIGH			(STACK_LOW + STACK_SIZE)

extern int errno;


void print_sp(Elf64_Addr sp)
{
	Elf64_Addr q = sp;
	printf("argc : %lld\n", *(long long *)q);
	q += sizeof(char *);
	int i = 0;
	for (char **p = (char **)q; ; p++, i++) {
		printf("argv[%d] (%p) : (%p) '%s'\n", i, p, *p, *p);
		if (!*p) {
			q = (Elf64_Addr)++p;
			break;
		}
	}
	i = 0;
	for (char **p = (char **)q; ; p++, i++) {
		printf("envp[%d] (%p) : (%p) '%s'\n", i, p, *p, *p);
		if (!*p) {
			q = (Elf64_Addr)++p;
			break;
		}
	}
	printf("envc: %d\n", i);
	i = 0;
    for (Elf64_auxv_t *p = (Elf64_auxv_t *)q; p->a_type != AT_NULL; p++, i++);
	printf("auxc: %d\n", i);
    puts("");
}
// type declaration: /include/uapi/linux/elf.h
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

Elf64_Addr create_elf_tables(char *argv[], char *envp[], int fd, Elf64_Ehdr elf_header, Elf64_Addr load_addr)
{
	Elf64_auxv_t *auxv;
	int argc, envc, auxc;

	// argc, argv
	argc = 0;
	for (char **p = argv; *p; p++)
		argc++;
	// envp
	envc = 0;
	for (char **p = envp; *p; p++)
		envc++;
	// auxv
	auxv = (Elf64_auxv_t *)(envp + envc + 1), auxc = 0;
	for (Elf64_auxv_t *p = auxv; p->a_type != AT_NULL; p++)
		auxc++;

    // debug
	/*int i;
	printf("argc : %d\n", argc);
	i = 0;
	for (char **p = argv; *p; p++, i++)
		printf("argv[%d] (%p) : (%p) '%s'\n", i, p, *p, *p);
	i = 0;
	for (char **p = envp; *p; p++, i++)
		printf("envp[%d] (%p) : (%p) '%s'\n", i, p, *p, *p);
	i = 0;
    for (Elf64_auxv_t *p = auxv; p->a_type != AT_NULL; p++, i++);
    printf("auxc: %d\n", i);
    puts("");
	 */

	
	Elf64_Addr sp;

	
	// stack -> [STACK_LOW, STACK_LOW + STACK_SIZE)
	sp = (Elf64_Addr)mmap((void *)STACK_LOW, STACK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
	sp += STACK_SIZE;  // sp = STACK_HIGH
	
	// auxv
	sp -= (auxc + 1) * sizeof(Elf64_auxv_t);
	memcpy((void *)sp, auxv, (auxc + 1) * sizeof(Elf64_auxv_t));
	for (Elf64_auxv_t *p = (Elf64_auxv_t *)sp; p->a_type != AT_NULL; p++) {
		switch (p->a_type) {
			case AT_PHDR:	// Program headers for program
				printf("AT_PHDR: %#lx -> %#lx\n", p->a_un.a_val, load_addr + elf_header.e_phoff);
				p->a_un.a_val = load_addr + elf_header.e_phoff;
				break;
			case AT_PHNUM:	// Number of program headers
				printf("AT_PHNUM: %lu -> %u\n", p->a_un.a_val, elf_header.e_phnum);
				p->a_un.a_val = elf_header.e_phnum;
				break;
			case AT_ENTRY:	// Entry point of program
				printf("AT_ENTRY: %#lx -> %#lx\n", p->a_un.a_val, elf_header.e_entry);
				p->a_un.a_val = elf_header.e_entry;
				break;
		}
	}

	// envp
	sp -= (envc + 1) * sizeof(void *);
	memcpy((void *)sp, envp, (envc + 1) * sizeof(char *));
	// argv
	sp -= (argc + 1) * sizeof(void *);
	memcpy((void *)sp, argv, (argc + 1) * sizeof(char *));
	// argc
	sp -= sizeof(void *);
	*(long long *)sp = (long long)argc;
	

	//print_sp(sp);  // debug

	return sp;
}





/*
void *elf_map(Elf64_Addr addr, Elf64_Xword len, int prot, int flags, int fd, Elf64_Off offset)
{
	void *retval;
	Elf64_Addr new_addr;
	Elf64_Xword new_len;
	Elf64_Off new_offset;

    // align to page
	// Loadable process segments must have congruent values for p_vaddr and p_offset, modulo the page size.
	new_addr = ELF_PAGESTART(addr);
	new_offset = ELF_PAGESTART(offset);
	new_len = ELF_PAGEALIGN(ELF_PAGEOFFSET(addr) + len);
	
	// debug
	fprintf(stderr, "mmap(addr=%#lx, len=%#lx) -> ", new_addr, new_len);
	
	// A file is mapped in multiples of the page size.
	// For a file that is not a multiple of the page size, 
	//		the remaining memory is zeroed when mapped, and writes to that region are not written out to the file.
	retval = mmap((void *)new_addr, new_len, prot, flags, fd, new_offset);
	if (retval == MAP_FAILED)
		perror("Error: mmap");
	else
		fprintf(stderr, "[%p, %p) mapped\n", retval, retval + new_len);
    return retval;
}

void *elf_map_bss(Elf64_Addr start, Elf64_Addr end, int prot, int flags)
{
	void *retval;
	Elf64_Addr new_start, new_end;

    // align to page
	new_start = ELF_PAGEALIGN(start), new_end = ELF_PAGEALIGN(end);
	if (new_start == new_end)
		return (void *)new_start;
	
	// debug
	fprintf(stderr, "<.bbs> mmap(addr=%#lx, len=%#lx) -> ", new_start, new_end);

	// MAP_ANONYMOUS: The mapping is not backed by any file; its contents are initialized to zero.
	retval = mmap((void *)new_start, end - start, prot, flags | MAP_ANONYMOUS, -1, 0);
	if (retval == MAP_FAILED)
		perror("Error: mmap");
	else
		fprintf(stderr, "[%p, %p) mapped\n", retval, retval + (new_end - new_start));
    return retval;
}


Elf64_Ehdr load_elf_binary(int fd, Elf64_Addr *load_addr)
{
	Elf64_Ehdr elf_header;

	Elf64_Phdr program_header_entry;
	Elf64_Addr elf_bss, elf_brk;
	int elf_prot;
	int elf_flags = MAP_PRIVATE | MAP_FIXED;	// ET_EXEC only

	int load_addr_set = 0;
	*load_addr = 0;


	// ELF header
	read(fd, &elf_header, sizeof(Elf64_Ehdr));
    print_elf_header(&elf_header);  // debug

    if (strncmp(&elf_header.e_ident[0], "\x7f""ELF", 4))  // magic number
		exit(1);
	if (elf_header.e_ident[EI_CLASS] != ELFCLASS64)
		exit(1);
    if (elf_header.e_type != ET_EXEC)	// only supported for ET_EXEC(static linked executable), not ET_DYN.
        exit(1);


    // Program header table
    lseek(fd, elf_header.e_phoff, SEEK_SET);
    for (int i = 0; i < elf_header.e_phnum; i++) {
        read(fd, &program_header_entry, sizeof(Elf64_Phdr));

        if (program_header_entry.p_type != PT_LOAD)
            continue;

		puts(""), print_program_header_entry(i, &program_header_entry);	// debug

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
        elf_bss = program_header_entry.p_vaddr + program_header_entry.p_filesz;
        elf_brk = program_header_entry.p_vaddr + program_header_entry.p_memsz;
        if (elf_bss < elf_brk) {  // !(elf_bss == elf_brk)
            elf_map_bss(elf_bss, elf_brk, elf_prot, elf_flags);
        }
    }

	return elf_header;
}

*/


void *elf_map(Elf64_Addr addr, int prot, int type, 
		int fd, Elf64_Phdr *pp)
{
	unsigned long size = pp->p_filesz + ELF_PAGEOFFSET(pp->p_vaddr);
	unsigned long off = pp->p_offset - ELF_PAGEOFFSET(pp->p_vaddr);
	addr = ELF_PAGESTART(addr);
	size = ELF_PAGEALIGN(size);

	if (!size)
		return (void *) addr;
	
	return mmap((void *) addr, size, prot, type, fd, off);
}

int map_bss(unsigned long start, unsigned long end, int prot)
{
	int flags;

	start = ELF_PAGEALIGN(start);
	end = ELF_PAGEALIGN(end);
	flags = MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE;
	if (end > start) {
		return (int) mmap((void *) start, end - start, prot, flags, -1, 0);
	}

	return 0;
}

int padzero(unsigned long elf_bss)
{
	unsigned long nbyte;

	nbyte = ELF_PAGEOFFSET(elf_bss);
	if (nbyte) {
		nbyte = PAGE_SIZE - nbyte;
		memset((void *) elf_bss, 0, nbyte);
	}

	return 0;
}

int load_elf_binary(int fd, Elf64_Ehdr *ep)
{
	Elf64_Phdr phdr;
	Elf64_Addr elf_entry;
	unsigned long elf_bss, elf_brk;
	int bss_prot = 0;
	int i;

	lseek(fd, ep->e_phoff, SEEK_SET);
	elf_bss = 0;
	elf_brk = 0;
	for (i = 0; i < ep->e_phnum; i++) {
		int elf_prot = 0, elf_flags;
		unsigned long k;
		Elf64_Addr vaddr;

		memset(&phdr, 0, sizeof(Elf64_Phdr));
		if (read(fd, &phdr, sizeof(Elf64_Phdr)) < 0) {
			fprintf(stderr, "read erorr on phdr\n");
			return -1;
		}
		
		if (phdr.p_type != PT_LOAD)
			continue;

		if (elf_brk > elf_bss) {
			if (map_bss(elf_bss, elf_brk, bss_prot) < 0) {
				fprintf(stderr, "map_bss error\n");
				return -1;
			}

			padzero(elf_bss);
		}

		if (phdr.p_flags & PF_R)
			elf_prot |= PROT_READ;
		if (phdr.p_flags & PF_W)
			elf_prot |= PROT_WRITE;
		if (phdr.p_flags & PF_X)
			elf_prot |= PROT_EXEC;
		
		elf_flags = MAP_PRIVATE | MAP_FIXED | MAP_EXECUTABLE;

		vaddr = phdr.p_vaddr;

		if (elf_map(vaddr, elf_prot, elf_flags, fd, &phdr) < 0) {
			fprintf(stderr, "elf_map error\n");
			return -1;
		}
		
		k = phdr.p_vaddr + phdr.p_filesz;
		if (k > elf_bss)
			elf_bss = k;

		k = phdr.p_vaddr + phdr.p_memsz;
		if (k > elf_brk) {
			bss_prot = elf_prot;
			elf_brk = k;
		}
	}

	if (map_bss(elf_bss, elf_brk, bss_prot) < 0) {
		fprintf(stderr, "bss map error\n");
		return -1;
	}
	if (elf_bss != elf_brk)
		padzero(elf_bss);

	elf_entry = ep->e_entry;

	return 0;
}





void start_thread(Elf64_Addr entry, Elf64_Addr sp)
{
	/*
...8:	argv[0] ->	argv[0]	-> 	argv[0]	->	argv[0]	-> 	argv[0]
...0:	argc					[]			[]			[]
											%rax		%rax
														&(%rax)
					
					// %rsi = argc
					// %rdx = (%rsp = &argv[0])
														// %r8  = _libc_csu_fini
														// %rcx = _libc_csu_init
														// %rdi = main

		_libc_start_main(
			%rdi=main, %rsi=argc, %rdx=argv, %rcx=_libc_csu_init, %r8=_libc_csu_fini, %r9
		)
	 */
	asm("movq $0, %rax");
	asm("movq $0, %rbx");
	asm("movq $0, %rcx");
	asm("movq $0, %rdx");
	asm("movq %0, %%rsp" : : "r" (sp));
	asm("jmp *%0" : : "c" (entry));
}

int my_execve(const char *path, char *argv[], char *envp[])
{
	int fd;
	Elf64_Ehdr elf_header;
	Elf64_Addr sp;

    if ((fd = open(argv[0], O_RDONLY)) == -1) {
        fprintf(stderr, "Error: open: %s\n", strerror(errno));
		goto out;
    }

	//
	read(fd, &elf_header, sizeof(Elf64_Ehdr));
	load_elf_binary(fd, &elf_header);
	sp = create_elf_tables(argv, envp, fd, elf_header, 0);
	//
	/*
	Elf64_Addr load_addr;
	elf_header = load_elf_binary(fd, &load_addr);
	sp = create_elf_tables(argv, envp, fd, elf_header, load_addr);
	 */

	close(fd);

	puts("");
	printf("%%rsp: %#lx\n", sp);
	printf("Entry address: %#lx\n", elf_header.e_entry);

	// context switch
	start_thread(elf_header.e_entry, sp);

out:
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

	if (execve(new_argv[0], new_argv, envp) == -1) {
		fprintf(stderr, "Cannot execute the program '%s': %s\n", new_argv[0], strerror(errno));
		exit(1);
	}

	printf("This is never printed\n");
	return 0;
}