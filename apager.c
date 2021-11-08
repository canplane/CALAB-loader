// cc -static test.c -o test.out  // disable dynamic linking. ET_DYN -> ET_EXEC

// gcc -S test.c -> test.s
// gcc -c test.c -> test.o
// od -t x1 test.o
// readelf [-h | -l] test.o
// objdump -d test.o

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <elf.h>

#define MAX2(a, b) 			((a) < (b) ? (b) : (a))

/* Adapted from linux/fs/binfmt_elf.c */
#define PAGE_SIZE			4096
#define ELF_MIN_ALIGN		PAGE_SIZE

#define ELF_PAGESTART(_v)	((_v) & ~(unsigned long)(ELF_MIN_ALIGN - 1))			// floor
#define ELF_PAGEOFFSET(_v)	((_v) & (ELF_MIN_ALIGN - 1))							// offset
#define ELF_PAGEALIGN(_v)	(((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))		// ceil

extern int errno;


Elf64_Addr create_elf_tables(char *argv[], char *envp[], int fd, Elf64_Ehdr elf_header)
{
	Elf64_auxv_t *auxv;
	int argc, envc, auxc;

	int stack_size;


	stack_size = 0;

	// argc, argv
	argc = 0;
	for (char **p = argv; *p; p++)
		argc++;
	stack_size += sizeof(int);
	stack_size += (argc + 1) * sizeof(char *);
	// envp
	envc = 0;
	for (char **p = envp; *p; p++)
		envc++;
	stack_size += (envc + 1) * sizeof(char *);
	// auxv
	auxv = (Elf64_auxv_t *)(envp + envc + 1), auxc = 0;
	for (Elf64_auxv_t *p = auxv; p->a_type != AT_NULL; p++)
		auxc++;
	stack_size += (auxc + 1) * sizeof(Elf64_auxv_t);


    /* // debug
	int i;
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
	sp = (Elf64_Addr)mmap((void *)0x10000000, ELF_PAGEALIGN(stack_size), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);  // MAP_GROUSDOWN
	sp += stack_size;

	// auxv
	sp -= (auxc + 1) * sizeof(Elf64_auxv_t);
	memcpy((void *)sp, auxv, (auxc + 1) * sizeof(Elf64_auxv_t));
	for (Elf64_auxv_t *p = (Elf64_auxv_t *)sp; p->a_type != AT_NULL; p++) {
		switch (p->a_type) {
			case AT_EXECFD:	// File descriptor of program
				//printf("AT_EXECFD: %ld -> %d\n", p->a_un.a_val, fd);
				p->a_un.a_val = fd;
				break;
			case AT_PHDR:	// Program headers for program
				//printf("AT_PHDR: %#lx -> %#lx\n", p->a_un.a_val, elf_header.e_phoff);
				p->a_un.a_val = elf_header.e_phoff;
				break;
			case AT_PHENT:	// Size of program header entry
				//printf("AT_PHENT: %lu -> %u\n", p->a_un.a_val, elf_header.e_phentsize);
				p->a_un.a_val = elf_header.e_phentsize;
				break;
			case AT_PHNUM:	// Number of program headers
				//printf("AT_PHNUM: %lu -> %u\n", p->a_un.a_val, elf_header.e_phnum);
				p->a_un.a_val = elf_header.e_phnum;
				break;
			case AT_BASE:
				//printf("AT_BASE: %#lx -> \n", p->a_un.a_val);
				break;
			case AT_ENTRY:	// Entry point of program
				//printf("AT_ENTRY: %#lx -> %#lx\n", p->a_un.a_val, elf_header.e_entry);
				p->a_un.a_val = elf_header.e_entry;
				break;
		}
	}

	// envp
	sp -= (envc + 1) * sizeof(char *);
	memcpy((void *)sp, envp, (envc + 1) * sizeof(char *));
	// argv
	sp -= (argc + 1) * sizeof(char *);
	memcpy((void *)sp, argv, (argc + 1) * sizeof(char *));
	// argc
	sp -= sizeof(int);
	memcpy((void *)sp, &argc, sizeof(int));
	

	/* // debug
	Elf64_Addr q = (Elf64_Addr)sp;
	printf("argc : %d\n", *(int *)q);
	q += sizeof(int);
	i = 0;
	for (char **p = (char **)q; ; p++, i++) {
		if (!*p) {
			q = (Elf64_Addr)++p;
			break;
		}
		printf("argv[%d] (%p) : (%p) '%s'\n", i, p, *p, *p);
	}
	i = 0;
	for (char **p = (char **)q; ; p++, i++) {
		if (!*p) {
			q = (Elf64_Addr)++p;
			break;
		}
		printf("envp[%d] (%p) : (%p) '%s'\n", i, p, *p, *p);
	}
	i = 0;
    for (Elf64_auxv_t *p = (Elf64_auxv_t *)q; p->a_type != AT_NULL; p++, i++) {
		if (p->a_type == AT_NULL) {
			printf("auxc: %d\n", i);
			break;
		}
	}
    puts("");
	 */

	//sp -= sizeof(Elf64_Addr);

	return sp;
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


Elf64_Ehdr load_elf_binary(int fd)
{
	Elf64_Ehdr elf_header;

	Elf64_Phdr program_header_entry;
	Elf64_Addr elf_bss, elf_brk;
	int elf_prot;
	int elf_flags = MAP_PRIVATE | MAP_FIXED;	// ET_EXEC only


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

        // .bss
        elf_bss = program_header_entry.p_vaddr + program_header_entry.p_filesz;
        elf_brk = program_header_entry.p_vaddr + program_header_entry.p_memsz;
        if (elf_bss < elf_brk) {  // !(elf_bss == elf_brk)
            elf_map_bss(elf_bss, elf_brk, elf_prot, elf_flags);
        }
    }

	return elf_header;
	
	


	/*bprm->p -= MAX_ARG_PAGES * PAGE_SIZE;
	create_elf_tables((char *)bprm->p, bprm->argc, bprm->envc, load_addr);

	sys_brk((elf_brk + 0xfff) & 0xfffff000);

	padzero(elf_bss);

	error = do_mmap(NULL, 0, 4096, PROT_READ | PROT_EXEC,
			MAP_FIXED | MAP_PRIVATE, 0);

	regs->eip = elf_entry;		// eip, magic happens :-)
	regs->esp = bprm->p;			// stack pointer
	return 0;*/
}

int my_execve(const char *path, char *argv[], char *envp[])
{
	int fd;
	Elf64_Ehdr elf_header;
	Elf64_Addr stack_top;

    if ((fd = open(argv[0], O_RDONLY)) == -1) {
        fprintf(stderr, "Error: open: %s\n", strerror(errno));
        exit(1);
    }

	elf_header = load_elf_binary(fd);
	stack_top = create_elf_tables(argv, envp, fd, elf_header);

	puts("");
	printf("Entry address: %#lx\n", elf_header.e_entry);
	
	asm("movq $0, %rax");
	asm("movq $0, %rcx");
	asm("movq $0, %rdx");
	asm("movq $0, %rbx");
	asm("movq %0, %%rsp" : : "r" (stack_top));
	asm("jmp *%0" : : "c" (elf_header.e_entry));

    
    // debug
    /*
	char *addr;
    int WORD = 16;
    for (int offset = 0; offset < statbuf.st_size; offset++) {
        if (offset % WORD == 0) {
            printf("\n%06d:", offset);
        }
        if (32 < addr[offset] && addr[offset] < 127) {
            printf(" %2c", addr[offset]);
        }
        else {
            printf(" %02x", (int)addr[offset] & 0x000000ff);
        }
    }
    printf("\n");
    printf("%lld\n", statbuf.st_size);
     */

	// munmap and close file
    /*if (munmap(addr, 40) == -1) {
        perror("Error: munmap\n");
        exit(1);
    }*/
    close(fd);

	return 0;
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

	

	printf("This is not to be printed\n");
	return 0;
}