/* 
 * /part-2/3-thread/apager.c
 * ----------------
 * CALAB Master Programming Project - (Part 2) Advanced user-level loader
 * User-level threading (all-at-once loading)
 * 
 * Sanghoon Lee (canplane@gmail.com)
 * 12 November 2021
 */


#ifndef			__APAGER_C__
#define			__APAGER_C__




#include 		"./common.c"




void map_segment(int thread_id, const Elf64_Phdr *pp, int fd) {
	Elf64_Addr segment_start, bss_start, bss_end;
	segment_start = pp->p_vaddr;
	bss_start = segment_start + pp->p_filesz, bss_end = segment_start + pp->p_memsz;

	int elf_prot = 0;
	if (pp->p_flags & PF_R)	elf_prot |= PROT_READ;
	if (pp->p_flags & PF_W)	elf_prot |= PROT_WRITE;
	if (pp->p_flags & PF_X)	elf_prot |= PROT_EXEC;

	int elf_flags = MAP_PRIVATE | MAP_FIXED;	// valid in ET_EXEC


	Elf64_Addr page_start, page_end, offset;
	
	// Loadable process segments must have congruent values for p_vaddr and p_offset, modulo the page size.
	page_start = PAGE_FLOOR(segment_start), page_end = PAGE_CEIL(bss_start);
	offset = PAGE_FLOOR(pp->p_offset);

	fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Segments (file offset = %#lx) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, offset, page_start, page_end - page_start);
	if (mmap((void *)page_start, page_end - page_start, elf_prot, elf_flags, fd, offset) == MAP_FAILED)
		goto mmap_err;
	

	// .bss: read-write zero-initialized anonymous memory
	if (bss_start < bss_end) {
		memset((void *)bss_start, 0, PAGE_CEIL(bss_start) - bss_start);		// zero-fill
		
		page_start = PAGE_CEIL(bss_start), page_end = PAGE_CEIL(bss_end);
		if (page_start < page_end) {
			fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Segments (.bss) -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, page_start, page_end - page_start);
			if (mmap((void *)page_start, page_end - page_start, elf_prot, elf_flags | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
				goto mmap_err;
		}
	}
	return;

mmap_err:
	perror("Error: Memory mapping failed");
	exit(1);
}

Elf64_Addr load_segments(int thread_id)
{
	Thread *th = &thread[thread_id];
	for (int i = 0; i < th->p_header_num; i++) {
		if (th->p_header_table[i].p_type != PT_LOAD)
            continue;

		map_segment(thread_id, &th->p_header_table[i], th->fd);
	}
}




int execves(const char *argv[], const char *envp[])
{
	int i;

	/* make additional envp to interact loader with thread */

	char *envp_added[8], envp_added_asciiz_space[256];
	make_additional_envp(envp_added, envp_added_asciiz_space);

	
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
			load_segments(i);	// for apager only

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
		
		//>>>>
		dispatch(i);	// context switch
		//<<<<

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