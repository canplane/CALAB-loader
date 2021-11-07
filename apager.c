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

#include <linux/elf.h>

#define MAX2(a, b) 			((a) < (b) ? (b) : (a))

/* Adapted from linux/fs/binfmt_elf.c */
#define PAGE_SIZE			4096
#define ELF_MIN_ALIGN		PAGE_SIZE

#define ELF_PAGESTART(_v)	((_v) & ~(unsigned long)(ELF_MIN_ALIGN - 1))			// floor
#define ELF_PAGEOFFSET(_v)	((_v) & (ELF_MIN_ALIGN - 1))							// offset
#define ELF_PAGEALIGN(_v)	(((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))		// ceil

extern int errno;
extern char **environ;	// get environment variables



/*unsigned long * create_elf_tables(char * p, int argc, int envc, Elf64_Ehdr* exec, unsigned int load_addr, int ibcs)
{
	unsigned long *argv, *envp, *dlinfo;
	unsigned long *sp;
	struct vm_area_struct *mpnt;

	mpnt = malloc(sizeof(struct vm_area_struct));
	mpnt->vm_task = current;
	mpnt->vm_start = PAGE_MASK & (unsigned long)p;
	mpnt->vm_end = TASK_SIZE;
	mpnt->vm_page_prot = PAGE_PRIVATE | PAGE_DIRTY;
	mpnt->vm_share = NULL;
	mpnt->vm_inode = NULL;
	mpnt->vm_offset = 0;
	mpnt->vm_ops = NULL;
	insert_vm_struct(current, mpnt);
	current->stk_vma = mpnt;
	
	sp = (unsigned long *)(0xfffffffc & (unsigned long)p);
	if (exec)
		sp -= DLINFO_ITEMS * 2;
	dlinfo = sp;
	sp -= envc + 1;
	envp = sp;
	sp -= argc + 1;
	argv = sp;
	if (!ibcs) {
		put_fs_long((unsigned long)envp, --sp);
		put_fs_long((unsigned long)argv, --sp);
	}*/

	/* The constant numbers (0-9) that we are writing here are
	   described in the header file sys/auxv.h on at least
	   some versions of SVr4 */
	//if (exec) { /* Put this here for an ELF program interpreter */
	/*	struct elf_phdr *eppnt;
		eppnt = (struct elf_phdr *)exec->e_phoff;
		put_fs_long(3, dlinfo++); put_fs_long(load_addr + exec->e_phoff, dlinfo++);
		put_fs_long(4, dlinfo++); put_fs_long(sizeof(struct elf_phdr), dlinfo++);
		put_fs_long(5, dlinfo++); put_fs_long(exec->e_phnum, dlinfo++);
		put_fs_long(9, dlinfo++); put_fs_long((unsigned long)exec->e_entry, dlinfo++);
		put_fs_long(7, dlinfo++); put_fs_long(SHM_RANGE_START, dlinfo++);
		put_fs_long(8, dlinfo++); put_fs_long(0, dlinfo++);
		put_fs_long(6, dlinfo++); put_fs_long(PAGE_SIZE, dlinfo++);
		put_fs_long(0, dlinfo++); put_fs_long(0, dlinfo++);
	};

	put_fs_long((unsigned long)argc, --sp);
	current->arg_start = (unsigned long)p;
	while (argc-- > 0) {
		put_fs_long((unsigned long)p, argv++);
		while (get_fs_byte(p++))
			;
	}
	put_fs_long(0, argv);
	current->arg_end = current->env_start = (unsigned long) p;
	while (envc-- > 0) {
		put_fs_long((unsigned long)p, envp++);
		while (get_fs_byte(p++))
			;
	}
	put_fs_long(0,envp);
	current->env_end = (unsigned long)p;
	return sp;
}*/


// type declaration: /include/uapi/linux/elf.h
void print_elf_header(Elf64_Ehdr *ep)
{
	printf("ELF header {\n");
	printf("\tunsigned char \te_ident[16]: \t%s\n", ep->e_ident);		// ELF identification
        printf("\tElf64_Half \te_type: \t%#x\n", ep->e_type);			// Object file type
        printf("\tElf64_Half \te_machine: \t%#x\n", ep->e_machine);			// Machine type
        printf("\tElf64_Word \te_version: \t%u\n", ep->e_version);			// Object file version
    printf("\tElf64_Addr \te_entry: \t%#llx\n", ep->e_entry);			// Entry point address
	printf("\tElf64_Off \te_phoff: \t%#llx\n", ep->e_phoff);			// Program header offset
	printf("\tElf64_Off \te_shoff: \t%#llx\n", ep->e_shoff);			// Section header offset
        //printf("\tElf64_Word \te_flags: \t%#x\n", ep->e_flags);			// Processor-specific flags
    printf("\tElf64_Half \te_ehsize: \t%u\n", ep->e_ehsize);			// ELF header size
    printf("\tElf64_Half \te_phentsize: \t%u\n", ep->e_phentsize);		// Size of program header entry
        // equal to sizeof(Elf64_Ehdr) = 64
	printf("\tElf64_Half \te_phnum: \t%d\n", ep->e_phnum);				// Number of program header entries
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
    printf("\tElf64_Off \tp_offset: \t%#llx\n", pp->p_offset);			// Offset in file
    printf("\tElf64_Addr \tp_vaddr: \t%#llx\n", pp->p_vaddr);			// Virtual address in memory
    	//printf("\tElf64_Addr \tp_paddr: \t%#llx\n", pp->p_paddr);			// Reserved
    printf("\tElf64_Xword \tp_filesz: \t%#llx\n", pp->p_filesz);		// Size of segment in file
    printf("\tElf64_Xword \tp_memsz: \t%#llx\n", pp->p_memsz);			// Size of segment in memory
    printf("\tElf64_Xword \tp_align: \t%#llx\n", pp->p_align);			// Alignment of segment
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
	fprintf(stderr, "mmap(addr=%#llx, len=%#llx) -> ", new_addr, new_len);
	
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
	fprintf(stderr, "<.bbs> mmap(addr=%#llx, len=%#llx) -> ", new_start, new_end);

	// MAP_ANONYMOUS: The mapping is not backed by any file; its contents are initialized to zero.
	retval = mmap((void *)new_start, end - start, prot, flags | MAP_ANONYMOUS, -1, 0);
	if (retval == MAP_FAILED)
		perror("Error: mmap");
	else
		fprintf(stderr, "[%p, %p) mapped\n", retval, retval + (new_end - new_start));
    return retval;
}


int load_elf_binary(int fd)
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
	
	/*


	bprm->p -= MAX_ARG_PAGES * PAGE_SIZE;
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
    char *addr;
    struct stat statbuf;

    //argv[0] = "test.o";

	// open file and set statbuf
	/*if (stat(argv[0], &statbuf) == -1) {
        fprintf(stderr, "Error: stat: %s\n", strerror(errno));
        exit(1);
    }*/
    if ((fd = open(argv[0], O_RDONLY)) == -1) {
        fprintf(stderr, "Error: open: %s\n", strerror(errno));
        exit(1);
    }
	// mmap
	/*addr = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);
    if (addr == MAP_FAILED) {
        perror("Error: mmap\n");
        exit(1);
    }*/

	load_elf_binary(fd);
    
    // debug
    /*
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

int main(int argc, char *argv[])
{
	char	**new_argv;

	if (argc < 2) {
        fprintf(stderr, "Usage: %s file [args ...]\n", argv[0]);
        exit(1);
    }

	new_argv = &argv[1];
	if (execve(new_argv[0], new_argv, environ) == -1) {
		fprintf(stderr, "Cannot execute the program '%s': %s\n", new_argv[0], strerror(errno));
		exit(1);
	}

	printf("This is not to be printed\n");
	return 0;
}