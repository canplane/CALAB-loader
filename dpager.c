#include 	"./common.c"
#include 	<signal.h>

extern int errno;



#define 	P_HEADER_START		STACK_HIGH



// set by load_elf_binary(), used by segv_handler()
int fd;

Elf64_Phdr *p_header_table;
int p_header_num;

Elf64_Ehdr read_elf_binary(const char *path)
{
	Elf64_Ehdr e_header;

	/* open the program */
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
	
	/* allocate memory to program header table */
	if (e_header.e_phnum * sizeof(Elf64_Phdr) > BASE_ADDR - STACK_HIGH) {
		fprintf(stderr, "Error: The size of program header table is too large to store into memory. Do not exceed %#lx.\n", BASE_ADDR - STACK_HIGH);
		exit(1);
	}
	//fprintf(stderr, "Mapping: program header table -> (memory address = %#lx, size = %#lx)\n", STACK_HIGH, PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)));
	if (mmap((void *)STACK_HIGH, PAGE_CEIL(e_header.e_phnum * sizeof(Elf64_Phdr)), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		perror("Error: Cannot allocate memory for a program header table");
		exit(1);
	}
	p_header_table = (Elf64_Phdr *)STACK_HIGH, p_header_num = e_header.e_phnum;		// or (p_headers = malloc(e_header.e_phnum * sizeof(Elf64_Phdr)))

	/* read program header table */
	if (lseek(fd, e_header.e_phoff, SEEK_SET) == -1) {
		perror("Error: lseek() failed");
		exit(1);
	}
	if (read(fd, p_header_table, e_header.e_phnum * sizeof(Elf64_Phdr)) == -1) {
		perror("Error: Cannot read a program header table");
		exit(1);
	}

	for (int i = 0; i < e_header.e_phnum; i++) {
		//fprintf(stderr, "Program header entry %d ", i), print_p_header(&p_headers[i]);
		if (p_header_table[i].p_type != PT_LOAD)
            continue;

		if (p_header_table[i].p_vaddr + p_header_table[i].p_memsz > STACK_LOW) {
			fprintf(stderr, "Error: Cannot support address range used by the program. This loader only supports for the range from %#lx to %#lx.\n", (size_t)0, STACK_LOW);
			exit(1);
		}
	}

	return e_header;
}



void map_one_page(Elf64_Addr addr, const Elf64_Phdr *pp) {
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
		fprintf(stderr, "Mapping: .bss -> (memory address = %#lx, size = %#lx)\n", page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
			goto mmap_err;
	}
	else if (page_end <= bss_start) {
		fprintf(stderr, "Mapping: (file offset = %#lx) -> (memory address = %#lx, size = %#lx)\n", offset, page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags, fd, offset) == MAP_FAILED)
			goto mmap_err;
	}
	else {	// page_start < bss_start < page_end
		fprintf(stderr, "Mapping: (file offset = %#lx) and .bss -> (memory address = %#lx, size = %#lx)\n", offset, page_start, PAGE_SIZE);
		if (mmap((void *)page_start, PAGE_SIZE, elf_prot, elf_flags, fd, offset) == MAP_FAILED)
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

void segv_handler(int signo, siginfo_t *si, void *arg)
{
	Elf64_Addr addr = (Elf64_Addr)si->si_addr;
    fprintf(stderr, "< Caught segmentation fault at address %#lx >\n", addr);

	for (int i = 0; i < p_header_num; i++) {
		if (p_header_table[i].p_type != PT_LOAD)
            continue;
		if (!((p_header_table[i].p_vaddr <= addr) && (addr < p_header_table[i].p_vaddr + p_header_table[i].p_memsz)))
			continue;
		map_one_page(addr, &p_header_table[i]);
		return;
	}

	// real violation (e.g., 0x0)
	fprintf(stderr, "Error: Invalid access at address %#lx\n", addr);
	exit(1);
}



struct sigaction sa;

void init_segv_handler()
{
	sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
}

int my_execve(const char *path, const char *argv[], const char *envp[])
{	
	Elf64_Ehdr e_header;
	Elf64_Addr sp;

	e_header = read_elf_binary(path);
	sp = create_stack(argv, envp, &e_header);
	//print_stack((const char **)(sp + sizeof(int64_t)));

	fprintf(stderr, "Executing the program '%s'... (Stack pointer = %#lx, Entry address = %#lx)\n", path, sp, e_header.e_entry);
	fprintf(stderr, "--------\n");
	
	init_segv_handler();	// set signal handler for segmentation fault

	start(e_header.e_entry, sp);	// context switch

	return -1;
}
#define 	execve 				my_execve



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