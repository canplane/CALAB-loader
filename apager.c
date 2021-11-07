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

extern int errno;
extern char **environ;	// get environment variables



unsigned long * create_elf_tables(char * p, int argc, int envc, Elf64_Ehdr* exec, unsigned int load_addr, int ibcs)
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
	}

	/* The constant numbers (0-9) that we are writing here are
	   described in the header file sys/auxv.h on at least
	   some versions of SVr4 */
	if (exec) { /* Put this here for an ELF program interpreter */
		struct elf_phdr *eppnt;
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
}

void print_elf_header(Elf64_Ehdr elf_header)
{
	printf("e_ident: %s\n", ep->e_ident);
	printf("e_entry: %p\n", ep->e_entry);
	printf("e_phoff: %lu\n", ep->e_phoff);
	printf("e_shoff: %lu\n", ep->e_shoff);
	printf("sizeof Elf64_Ehdr: %lu\n", sizeof(Elf64_Ehdr));
	printf("e_ehsize: %u\n", ep->e_ehsize);
	printf("e_phentsize: %u\n", ep->e_phentsize);
	printf("e_phnum: %u\n", ep->e_phnum);
}


int load_elf_binary(int fd)//, struct linux_binprm *bprm, struct pt_regs *regs)
{
	// struct elfhdr -> Elf64_Ehdr
	// struct elf_phdr -> Elf64_Phdr

	Elf64_Ehdr elf_header;
	struct file *file;
	unsigned int load_addr;
	int i;
	int error;

	Elf64_Phdr *elf_phdata, *elf_ppnt;
	int elf_exec_fileno;
	unsigned int elf_bss, k, elf_brk;
	int retval;
	Elf64_Addr elf_entry;
	unsigned long start_code, end_code, end_data;
	unsigned long elf_stack;
	
	load_addr = 0;
	
	/* ELF header */
	read(fd, &elf_header, sizeof(Elf64_Ehdr));
	printf_elf_header(elf_header);
	
	/*if (strncmp(&elf_header.e_ident[0], "\x7f""ELF", 4))  // magic #
		goto out;
	if ()
	
	// Program header
	elf_phdata = malloc(elf_ex.e_phentsize * elf_ex.e_phnum);
	lseek(bprm->file, elf_ex.e_phoff, SEEK_SET);
	read(bprm->file, (char *)elf_phdata, elf_ex.e_phentsize * elf_ex.e_phnum);

	elf_bss = 0;
	elf_brk = 0;
	
	elf_exec_fileno = open(bprm->file, O_RDONLY);

	file = current->filp[elf_exec_fileno];
	
	elf_stack = ~0UL;  // 0xff..ff
	start_code = 0;
	end_code = 0;
	end_data = 0;
	
	elf_entry = elf_ex.e_entry;
	
	bprm->p += change_ldt(0, bprm->page);
	current->start_stack = bprm->p;

	for (i = 0, elf_ppnt = elf_phdata; i < elf_ex.e_phnum; i++, elf_ppnt++) {
		if (elf_ppnt->p_type != PT_LOAD)
			continue;

		error = do_mmap(file,
				elf_ppnt->p_vaddr & 0xfffff000,
				elf_ppnt->p_filesz + (elf_ppnt->p_vaddr & 0xfff),
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_FIXED | MAP_PRIVATE,
				elf_ppnt->p_offset & 0xfffff000);
		
		if (!load_addr)
			load_addr = elf_ppnt->p_vaddr - elf_ppnt->p_offset;
		k = elf_ppnt->p_vaddr;
		if (k > start_code)
			start_code = k;
		k = elf_ppnt->p_vaddr + elf_ppnt->p_filesz;
		if (k > elf_bss)
			elf_bss = k;
		if ((elf_ppnt->p_flags | PROT_WRITE) && end_code < k)
			end_code = k;
		if (end_data < k)
			end_data = k; 
		k = elf_ppnt->p_vaddr + elf_ppnt->p_memsz;
		if (k > elf_brk)
			elf_brk = k;
	}

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
	if (stat(argv[0], &statbuf) == -1) {
        fprintf(stderr, "Error: stat: %s\n", strerror(errno));
        exit(1);
    }
    if ((fd = open(argv[0], O_RDWR | O_CREAT)) == -1) {
        fprintf(stderr, "Error: open: %s\n", strerror(errno));
        exit(1);
    }
	// mmap
	addr = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)0);
    if (addr == MAP_FAILED) {
        perror("Error: mmap\n");
        exit(1);
    }
    
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
    

    //load_elf_binary();

	// munmap and close file
    if (munmap(addr, 40) == -1) {
        perror("Error: munmap\n");
        exit(1);
    }
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