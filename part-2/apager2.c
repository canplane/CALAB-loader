#ifndef			__APAGER2_C__
#define			__APAGER2_C__



#include 		"./common.c"



void map_segment(const Elf64_Phdr *pp, int fd) {
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

	fprintf(stderr, "Mapping: (file offset = %#lx) -> (memory address = %#lx, size = %#lx)\n", offset, page_start, page_end - page_start);
	if (mmap((void *)page_start, page_end - page_start, elf_prot, elf_flags, fd, offset) == MAP_FAILED)
		goto mmap_err;
	

	// .bss: read-write zero-initialized anonymous memory
	if (bss_start < bss_end) {
		memset((void *)bss_start, 0, PAGE_CEIL(bss_start) - bss_start);		// zero-fill
		
		page_start = PAGE_CEIL(bss_start), page_end = PAGE_CEIL(bss_end);
		if (page_start < page_end) {
			fprintf(stderr, "Mapping: .bss -> (memory address = %#lx, size = %#lx)\n", page_start, page_end - page_start);
			if (mmap((void *)page_start, page_end - page_start, elf_prot, elf_flags | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
				goto mmap_err;
		}
	}
	return;

mmap_err:
	perror("Error: Memory mapping failed");
	exit(1);
}

Elf64_Ehdr load_elf_binary(const char *path)
{
	Elf64_Ehdr e_header;
	
	/* open the program */
	int fd;
	if ((fd = open(path, O_RDONLY)) == -1) {
        fprintf(stderr, "Error: Cannot open the program '%s': %s\n", path, strerror(errno));
		exit(1);
    }

	/* read ELF header */
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
	
	/* read program header table and map into memory */
	if (lseek(fd, e_header.e_phoff, SEEK_SET) == -1) {
		perror("Error: lseek() failed");
		exit(1);
	}
	
	Elf64_Phdr p_header;
	for (int i = 0; i < e_header.e_phnum; i++) {
		if (read(fd, &p_header, sizeof(Elf64_Phdr)) == -1) {
			perror("Error: Cannot read a program header entry");
			exit(1);
		}
		//fprintf(stderr, "Program header entry %d ", i), print_p_header(&p_header);

		if (p_header.p_type != PT_LOAD)
            continue;
		
		if (p_header.p_vaddr + p_header.p_memsz > STACK_LOW) {
			fprintf(stderr, "Error: Cannot support address range used by the program. This loader only supports for the range from %#lx to %#lx.\n", (size_t)0, STACK_LOW);
			exit(1);
		}
		map_segment(&p_header, fd);
	}

	close(fd);

	return e_header;
}


void init_jmpbuf(Elf64_Addr page_start, const char *argv[])
{
	int argc;
	for (argc = 0; argv[argc]; argc++)
		;
	Elf64_Addr page_end = page_start + PAGE_CEIL((argc + 1) * sizeof(jmp_buf));
	mmap((void *)page_start, page_end - page_start, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

	jmpbuf_p = (jmp_buf *)page_start;
}



int my_execve(const char *path, const char *argv[], const char *envp[])
{
	Elf64_Ehdr e_header;
	Elf64_Addr sp;
		
	e_header = load_elf_binary(path);

	init_jmpbuf(STACK_HIGH, argv);
	sp = create_stack(argv, envp, &e_header);
	//print_stack((const char **)(sp + sizeof(int64_t)));

	fprintf(stderr, "Executing the program '%s'... (Stack pointer = %#lx, Entry address = %#lx)\n", path, sp, e_header.e_entry);
	fprintf(stderr, "--------\n");
	
	if (!setjmp(jmpbuf_p[0])) {
		start(e_header.e_entry, sp);	// context switch
	}

	fprintf(stderr, "--------\n");
	fprintf(stderr, "The program '%s' ended\n", path);

	// unmap

	return 0;
}
#define 		execve 				my_execve



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
	return 0;
}