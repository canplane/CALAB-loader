#ifndef		__DPAGER_C__
#define		__DPAGER_C__



#include 	"./common.c"

#include 	<signal.h>




void map_one_page(Elf64_Addr addr, const Elf64_Phdr *pp, int elf_fd) {
	Elf64_Addr segment_start, bss_start, bss_end;
	segment_start = pp->p_vaddr;
	bss_start = segment_start + pp->p_filesz, bss_end = segment_start + pp->p_memsz;

	int elf_prot = 0;
	if (pp->p_flags & PF_R)	elf_prot |= PROT_READ;
	if (pp->p_flags & PF_W)	elf_prot |= PROT_WRITE;
	if (pp->p_flags & PF_X)	elf_prot |= PROT_EXEC;

	int elf_flags = MAP_PRIVATE | MAP_FIXED;	// valid in ET_EXEC


	Elf64_Addr page_start, page_end, offset;
	page_start = PAGE_FLOOR(addr), page_end = PAGE_FLOOR(addr) + PAGE_SIZE;
	offset = PAGE_FLOOR(pp->p_offset + (addr - segment_start));

	if (bss_start <= page_start) {
		fprintf(stderr, ERR_STYLE__"Mapping: Segments (.bss) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
			goto mmap_err;
	}
	else if (page_end <= bss_start) {
		fprintf(stderr, ERR_STYLE__"Mapping: Segments (file offset = %#lx) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, offset, page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags, elf_fd, offset) == MAP_FAILED)
			goto mmap_err;
	}
	else {	// page_start < bss_start < page_end
		fprintf(stderr, ERR_STYLE__"Mapping: Segments (file offset = %#lx), .bss) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, offset, page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags, elf_fd, offset) == MAP_FAILED)
			goto mmap_err;
		
		// zero-fill
		if (bss_start < bss_end)
			memset((void *)bss_start, 0, PAGE_CEIL(bss_start) - bss_start);
	}
	return;

mmap_err:
	perror("Error: Memory mapping failed");
	exit(1);
}

// custom segmentation fault handler
void page_fault_handler(int signo, siginfo_t *si, void *arg)
{
	Elf64_Addr addr = (Elf64_Addr)si->si_addr;
    fprintf(stderr, UND_STYLE__ ERR_STYLE__"< Caught segmentation fault at address %#lx >\n"__ERR_STYLE, addr);

	for (int i = 0; i < p_header_num; i++) {
		if (p_header_table[i].p_type != PT_LOAD)
            continue;
		if (!((p_header_table[i].p_vaddr <= addr) && (addr < p_header_table[i].p_vaddr + p_header_table[i].p_memsz)))
			continue;
		map_one_page(addr, &p_header_table[i], fd);
		return;
	}

	// real violation (e.g., 0x0)
	fprintf(stderr, "Error: Invalid access at address %#lx\n", addr);
	exit(1);
}




struct sigaction sa;

void set_page_fault_handler()
{
	sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = page_fault_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		perror("Error: Cannot activate page fault handler");
		exit(1);
	}
}




int my_execve(const char *path, const char *argv[], const char *envp[])
{	
	Elf64_Ehdr e_header;
	Elf64_Addr sp;

	e_header = read_elf_binary(path);
	sp = create_stack(argv, envp, &e_header);

	fprintf(stderr, INV_STYLE__ ERR_STYLE__" Executing program '%s' ... (stack pointer = %#lx, entry address = %#lx) \n"__ERR_STYLE, path, sp, e_header.e_entry);
	fprintf(stderr, ERR_STYLE__"--------\n"__ERR_STYLE);
	
	set_page_fault_handler();		// set signal handler for segmentation fault
	//>>>>
	start(e_header.e_entry, sp);	// context switch
	//<<<<

	return -1;
}
#define 		execve 									my_execve




#endif




int main(int argc, const char **argv, const char **envp)
{
	if (argc < 2) {
        fprintf(stderr, "Usage: %s file [args ...]\n", argv[0]);
        exit(1);
    }
	if (execve(argv[1], argv + 1, envp) == -1) {
		fprintf(stderr, "Cannot execute the program '%s': %s\n", argv[1], strerror(errno));
		exit(1);
	}

	printf("This is never printed.\n");
	return 0;
}