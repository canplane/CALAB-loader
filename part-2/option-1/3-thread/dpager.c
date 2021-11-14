/* 
 * /part-2/option-1/3-thread/dpager.c
 * ----------------
 * CALAB Master Programming Project - (Part 2) Advanced user-level loader
 * User-level threading (demand loading)
 * 
 * Sanghoon Lee (canplane@gmail.com)
 * 12 November 2021
 */


#ifndef			__DPAGER_C__
#define			__DPAGER_C__




#include 		"./common.c"

#include 		<signal.h>




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
    fprintf(stderr, UND_STYLE__ ERR_STYLE__"Thread %d: < Caught segmentation fault at address %#lx >\n"__ERR_STYLE, current_thread_idx, addr);

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
	execves(argv + 1, envp);
	
	return 0;
}