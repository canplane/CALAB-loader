/* 
 * /part-2/option-2/2-back_to_back/apager.c
 * ----------------
 * CALAB Master Programming Project - (Part 2) Advanced user-level loader
 * Option 2 (Using fork(2))
 * Back-to-back loading (all-at-once loading)
 * 
 * Sanghoon Lee (canplane@gmail.com)
 * 14 November 2021
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
	for (int i = 0; i < thread.p_header_num; i++) {
		if (thread.p_header_table[i].p_type != PT_LOAD)
            continue;

		map_segment(thread_id, &thread.p_header_table[i], thread.fd);
	}
}




int execves(const char *argv[], const char *envp[])
{
	int i;

	/* run threads */

	Elf64_Ehdr e_header;
	for (int i = 0; argv[i]; i++) {
		e_header = read_elf_binary(i, argv[i]);
		load_segments(i);	// for apager only

		thread.entry = e_header.e_entry;
		thread.sp = create_stack(i, argv, envp, &e_header);

		fprintf(stderr, INV_STYLE__ ERR_STYLE__" Executing thread %d ('%s') ... (stack pointer = %#lx, entry address = %#lx) \n"__ERR_STYLE, i, argv[i], thread.sp, thread.entry);
		fprintf(stderr, ERR_STYLE__"--------\n"__ERR_STYLE);
		
		//>>>>
		dispatch(i);	// context switch
		//<<<<

		fprintf(stderr, ERR_STYLE__"--------\n"__ERR_STYLE);
		fprintf(stderr, INV_STYLE__ ERR_STYLE__" Thread %d ('%s') ended with exit code %d \n"__ERR_STYLE, i, argv[i], thread.exit_code);
		unmap_thread(i);
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