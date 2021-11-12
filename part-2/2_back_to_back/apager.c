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

Elf64_Ehdr load_elf_binary(int thread_id, const char *path)
{
	Elf64_Addr addr_lower_bound, addr_upper_bound;
	addr_lower_bound = SEGMENT_SPACE_LOW;
	addr_upper_bound = SEGMENT_SPACE_HIGH;

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
	fprintf(stderr, ERR_STYLE__"Thread %d: Mapping: Program header table -> (memory address = %#lx, size = %#lx)\n"__ERR_STYLE, thread_id, P_HEADER_TABLE_LOW, PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)));
	if (mmap((void *)P_HEADER_TABLE_LOW, PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Cannot allocate program header table into memory");
		exit(1);
	}
	p_header_table = (Elf64_Phdr *)P_HEADER_TABLE_LOW;
	

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
			map_segment(thread_id, &p_header_table[i], fd);
		else {
			fprintf(stderr, "Error: Cannot support address range used by the program. This loader only supports for the range from %#lx to %#lx. Try gcc option '-Ttext-segment=%#lx'.\n", addr_lower_bound, addr_upper_bound, addr_lower_bound);
			exit(1);
		}
	}

	thread.fd = fd;
	thread.p_header_table = p_header_table, thread.p_header_num = e_header.e_phnum;
	
	return e_header;
}



int my_execve(const char *argv[], const char *envp[])
{
	int i;

	/* make additional envp to interact loader with thread */

	char *envp_added[8], envp_added_asciiz_space[256];
	make_additional_envp(envp_added, envp_added_asciiz_space);


	/* run threads */

	Elf64_Ehdr e_header;
	for (int i = 0; argv[i]; i++) {
		thread.state = THREAD_STATE_NEW;


		e_header = load_elf_binary(i, argv[i]);

		thread.entry = e_header.e_entry;
		thread.sp = create_stack(i, argv, envp, (const char **)envp_added, &e_header);
		//print_stack((const char **)(thread.sp + sizeof(unsigned long)));

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
#define 		execve 									my_execve



#endif



int main(int argc, const char **argv, const char **envp)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s file [args ...]\n", argv[0]);
		exit(1);
	}
	if (execve(argv + 1, envp) == -1) {
		fprintf(stderr, "Cannot execute the program '%s': %s\n", argv[1], strerror(errno));
		exit(1);
	}
	return 0;
}