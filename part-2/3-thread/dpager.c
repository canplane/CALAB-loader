#ifndef			__DPAGER_C__
#define			__DPAGER_C__




#include 		"./common.c"
#include 		<signal.h>




Elf64_Ehdr read_elf_binary(int thread_id, const char *path)
{
	Elf64_Addr addr_lower_bound, addr_upper_bound;
	addr_lower_bound = SEGMENT_SPACE_LOW(thread_id);
	addr_upper_bound = SEGMENT_SPACE_HIGH(thread_id);

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

	for (int i = 0; i < e_header.e_phnum; i++) {
		//fprintf(stderr, "Program header entry %d ", i), print_p_header(&p_header_table[i]);
		if (p_header_table[i].p_type != PT_LOAD)
            continue;

		if ((addr_lower_bound <= p_header_table[i].p_vaddr) && (p_header_table[i].p_vaddr + p_header_table[i].p_memsz <= addr_upper_bound))
			;
		else {
			fprintf(stderr, "Error: Cannot support address range used by the program. This loader only supports for the range from %#lx to %#lx. Try gcc option '-Ttext-segment=%#lx'.\n", addr_lower_bound, addr_upper_bound, addr_lower_bound);
			exit(1);
		}
	}

	thread[thread_id].fd = fd;
	thread[thread_id].p_header_table = p_header_table, thread[thread_id].p_header_num = e_header.e_phnum;

	return e_header;
}




void map_one_page(int thread_id, Elf64_Addr addr, const Elf64_Phdr *pp, int elf_fd) {
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
		fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Segments (.bss) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
			goto mmap_err;
	}
	else if (page_end <= bss_start) {
		fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Segments (file offset = %#lx) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, offset, page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags, elf_fd, offset) == MAP_FAILED)
			goto mmap_err;
	}
	else {	// page_start < bss_start < page_end
		fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Segments (file offset = %#lx, .bss) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, offset, page_start, PAGE_SIZE);
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
    fprintf(stderr, ERR_STYLE__"Thread %d: < Caught segmentation fault at address %#lx >\n"__ERR_STYLE, current_thread_idx, addr);

	Thread *th = &thread[current_thread_idx];
	for (int i = 0; i < th->p_header_num; i++) {
		if (th->p_header_table[i].p_type != PT_LOAD)
            continue;
		if (!((th->p_header_table[i].p_vaddr <= addr) && (addr < th->p_header_table[i].p_vaddr + th->p_header_table[i].p_memsz)))
			continue;
		map_one_page(current_thread_idx, addr, &th->p_header_table[i], th->fd);
		return;
	}

	// real violation (e.g., 0x0)
	fprintf(stderr, "Error: Invalid access at address %#lx\n", addr);
	exit(1);
}


struct sigaction sa;

void init_page_fault_handler()
{
	sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = page_fault_handler;
}
void activate_page_fault_handler(bool b)
{
	if (b)
		sa.sa_flags = SA_SIGINFO;
	else
		sa.sa_flags = SA_RESETHAND;
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		fprintf(stderr, "Error: Cannot %sactivate page fault handler: %s\n", b ? "" : "de", strerror(errno));
		exit(1);
	}
}




int execves(const char *argv[], const char *envp[])
{
	int i;

	/* make additional envp to interact loader with thread */

	char *envp_added[8], envp_added_asciiz_space[256];
	make_additional_envp(envp_added, envp_added_asciiz_space);


	/* init page fault handler */

	init_page_fault_handler();


	/* init ready queue */
	
	int _ready_q_array[THREAD_MAX_NUM + 1];
	Queue ready_q = Queue__init(_ready_q_array);
	
	for (i = 0; argv[i]; i++) {
		thread[i].state = THREAD_STATE_NEW;
		Queue__push(&ready_q, i);
	}
	if (i > THREAD_MAX_NUM) {
		fprintf(stderr, ERR_STYLE__"Warning: Can execute at most %d threads\n"__ERR_STYLE, THREAD_MAX_NUM);
	}


	/* run threads */

	Elf64_Ehdr e_header;
	while (!Queue__empty(&ready_q)) {
		i = Queue__front(&ready_q, int), Queue__pop(&ready_q);

		if (thread[i].state == THREAD_STATE_NEW) {
			e_header = read_elf_binary(i, argv[i]);

			thread[i].entry = e_header.e_entry;
			thread[i].sp = create_stack(i, argv, envp, (const char **)envp_added, &e_header);
			//print_stack((const char **)(thread[i].sp + sizeof(unsigned long)));
		}

		switch (thread[i].state) {
			case THREAD_STATE_NEW:
				fprintf(stderr, INV_STYLE__ ERR_STYLE__" Executing thread %d ('%s') ... (stack pointer = %#lx, entry address = %#lx) \n"__ERR_STYLE, i, argv[i], thread[i].sp, thread[i].entry);
				break;
			case THREAD_STATE_WAIT:
				fprintf(stderr, INV_STYLE__ ERR_STYLE__" Continuing thread %d ('%s') ... \n"__ERR_STYLE, i, argv[i]);
				break;
			default:
				fprintf(stderr, "Error: Invalid state before dispatch: %d\n", thread[i].state);
				exit(1);
				break;
		}
		fprintf(stderr, ERR_STYLE__"--------\n"__ERR_STYLE);

		activate_page_fault_handler(true);
		//>>>>
		dispatch(i);	// context switch
		//<<<<
		activate_page_fault_handler(false);

		fprintf(stderr, ERR_STYLE__"--------\n"__ERR_STYLE);
		switch (thread[i].state) {
			case THREAD_STATE_WAIT:
				fprintf(stderr, INV_STYLE__ ERR_STYLE__" Thread %d ('%s') waited \n"__ERR_STYLE, i, argv[i]);
				Queue__push(&ready_q, i);
				break;
			case THREAD_STATE_EXIT:
				fprintf(stderr, INV_STYLE__ ERR_STYLE__" Thread %d ('%s') ended with exit code %d \n"__ERR_STYLE, i, argv[i], thread[i].exit_code);
				unmap_thread(i);
				break;
			default:
				fprintf(stderr, "Error: Returned invalid state: %d\n", thread[i].state);
				exit(1);
				break;
		}
	}
	fprintf(stderr,  INV_STYLE__ ERR_STYLE__" All threads are successfully terminated \n"__ERR_STYLE);

	return 0;
}




#endif




int main(int argc, const char **argv, const char **envp)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s file [args ...]\n", argv[0]);
		exit(1);
	}
	if (execves(argv + 1, envp) == -1) {
		fprintf(stderr, "Cannot execute the program '%s': %s\n", argv[1], strerror(errno));
		exit(1);
	}
	return 0;
}