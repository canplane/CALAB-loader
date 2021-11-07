int load_elf_binary(struct linux_binprm *bprm, struct pt_regs *regs)
{
	// struct elfhdr -> Elf64_Ehdr
	// struct elf_phdr -> Elf64_Phdr


	Elf64_Ehdr elf_ex;
	Elf64_Ehdr interp_elf_ex;
	struct file *file;
  	struct exec interp_ex;
	struct inode *interpreter_inode;
	unsigned int load_addr;
	unsigned int interpreter_type = INTERPRETER_NONE;
	int i;
	int old_fs;
	int error;

	Elf64_Phdr *elf_phdata, *elf_ppnt;
	int elf_exec_fileno;
	unsigned int elf_bss, k, elf_brk;
	int retval;
	char * elf_interpreter;
	unsigned int elf_entry;
	int status;
	unsigned long start_code, end_code, end_data;
	unsigned long elf_stack;
	char passed_fileno[6];
	
	status = 0;
	load_addr = 0;
	
	// ELF header
	elf_ex = *((Elf64_Ehdr *)bprm->buf);
	
	// First of all, some simple consistency checks
	// (EI_MAG0..3: magic #) e_ident[0..3] = { '\x7f', 'E', 'L', 'F' }
	if (elf_ex.e_ident[0] != '\x7f' || strncmp(&elf_ex.e_ident[1], "ELF", 3) != 0)
		return -ENOEXEC;
	
	/*if (elf_ex.e_type != ET_EXEC || 
	   (elf_ex.e_machine != EM_386 && elf_ex.e_machine != EM_486) ||
	   (!bprm->inode->i_op || !bprm->inode->i_op->default_file_ops ||
	    !bprm->inode->i_op->default_file_ops->mmap)){
		return -ENOEXEC;
	};*/
	
	/* Now read in all of the header information */
	
	elf_phdata = malloc(elf_ex.e_phentsize * elf_ex.e_phnum);
	
	/*old_fs = get_fs();
	set_fs(get_ds());*/
	retval = read(bprm->inode, elf_ex.e_phoff, (char *)elf_phdata,
			   elf_ex.e_phentsize * elf_ex.e_phnum);
	//set_fs(old_fs);
	/*if (retval < 0) {
	        kfree (elf_phdata);
		return retval;
	}*/
	
	elf_ppnt = elf_phdata;
	elf_bss = 0;
	elf_brk = 0;
	
	elf_exec_fileno = open_inode(bprm->inode, O_RDONLY);

	if (elf_exec_fileno < 0) {
	        kfree (elf_phdata);
		return elf_exec_fileno;
	}
	
	file = current->filp[elf_exec_fileno];
	
	elf_stack = ~0UL;  // 0xff..ff
	elf_interpreter = NULL;
	start_code = 0;
	end_code = 0;
	end_data = 0;
	
	/*old_fs = get_fs();
	set_fs(get_ds());*/
	
	/*for (i = 0; i < elf_ex.e_phnum; i++){
		if (elf_ppnt->p_type == PT_INTERP) {
			// This is the program interpreter used for shared libraries - 
			// for now assume that this is an a.out format binary
			
			elf_interpreter = (char *) kmalloc(elf_ppnt->p_filesz, 
							   GFP_KERNEL);
			
			retval = read_exec(bprm->inode,elf_ppnt->p_offset,elf_interpreter,
					   elf_ppnt->p_filesz);
#if 0
			printk("Using ELF interpreter %s\n", elf_interpreter);
#endif
			if(retval >= 0)
				retval = namei(elf_interpreter, &interpreter_inode);
			if(retval >= 0)
				retval = read_exec(interpreter_inode,0,bprm->buf,128);
			
			if(retval >= 0){
				interp_ex = *((struct exec *) bprm->buf);		// exec-header
				interp_elf_ex = *((struct elfhdr *) bprm->buf);	  // exec-header
				
			};
			if(retval < 0) {
			  kfree (elf_phdata);
			  kfree(elf_interpreter);
			  return retval;
			};
		};
		elf_ppnt++;
	};*/
	
	//set_fs(old_fs);
	
	/* Some simple consistency checks for the interpreter */
	/*if(elf_interpreter){
	        interpreter_type = INTERPRETER_ELF | INTERPRETER_AOUT;
		if(retval < 0) {
			kfree(elf_interpreter);
			kfree(elf_phdata);
			return -ELIBACC;
		};
		// Now figure out which format our binary is
		if((N_MAGIC(interp_ex) != OMAGIC) && 
		   (N_MAGIC(interp_ex) != ZMAGIC) &&
		   (N_MAGIC(interp_ex) != QMAGIC)) 
		  interpreter_type = INTERPRETER_ELF;

		if (interp_elf_ex.e_ident[0] != 0x7f ||
		    strncmp(&interp_elf_ex.e_ident[1], "ELF",3) != 0)
		  interpreter_type &= ~INTERPRETER_ELF;

		if(!interpreter_type)
		  {
		    kfree(elf_interpreter);
		    kfree(elf_phdata);
		    return -ELIBBAD;
		  };
	}*/
	
	/* OK, we are done with that, now set up the arg stuff,
	   and then start this sucker up */
	
	/*if (!bprm->sh_bang) {
		char * passed_p;
		
		if(interpreter_type == INTERPRETER_AOUT) {
		  sprintf(passed_fileno, "%d", elf_exec_fileno);
		  passed_p = passed_fileno;
		
		  if(elf_interpreter) {
		    bprm->p = copy_strings(1,&passed_p,bprm->page,bprm->p,2);
		    bprm->argc++;
		  };
		};
		if (!bprm->p) {
		        if(elf_interpreter) {
			      kfree(elf_interpreter);
			}
		        kfree (elf_phdata);
			return -E2BIG;
		}
	}*/
	
	/* OK, This is the point of no return */
	//flush_old_exec(bprm);

	current->end_data = 0;
	current->end_code = 0;
	current->start_mmap = ELF_START_MMAP;
	current->mmap = NULL;
	elf_entry = (unsigned int) elf_ex.e_entry;
	
	/* Do this so that we can load the interpreter, if need be.  We will
	   change some of these later */
	current->rss = 0;
	bprm->p += change_ldt(0, bprm->page);
	current->start_stack = bprm->p;
	
	/* Now we do a little grungy work by mmaping the ELF image into
	   the correct location in memory.  At this point, we assume that
	   the image should be loaded at fixed address, not at a variable
	   address. */
	
	/*old_fs = get_fs();
	set_fs(get_ds());*/
	
	elf_ppnt = elf_phdata;
	for (i = 0; i < elf_ex.e_phnum; i++) {
		
		/*if(elf_ppnt->p_type == PT_INTERP) {
			// Set these up so that we are able to load the interpreter
		  // Now load the interpreter into user address space
		  set_fs(old_fs);

		  if(interpreter_type & 1) elf_entry = 
		    load_aout_interp(&interp_ex, interpreter_inode);

		  if(interpreter_type & 2) elf_entry = 
		    load_elf_interp(&interp_elf_ex, interpreter_inode);

		  old_fs = get_fs();
		  set_fs(get_ds());

		  iput(interpreter_inode);
		  kfree(elf_interpreter);
			
		  if(elf_entry == 0xffffffff) { 
		    printk("Unable to load interpreter\n");
		    kfree(elf_phdata);
		    send_sig(SIGSEGV, current, 0);
		    return 0;
		  };
		};*/
		
		
		if (elf_ppnt->p_type == PT_LOAD) {
			error = do_mmap(file,
					elf_ppnt->p_vaddr & 0xfffff000,
					elf_ppnt->p_filesz + (elf_ppnt->p_vaddr & 0xfff),
					PROT_READ | PROT_WRITE | PROT_EXEC,
					MAP_FIXED | MAP_PRIVATE,
					elf_ppnt->p_offset & 0xfffff000);
			
#ifdef LOW_ELF_STACK
			if(elf_ppnt->p_vaddr & 0xfffff000 < elf_stack) 
				elf_stack = elf_ppnt->p_vaddr & 0xfffff000;
#endif
			
			if (!load_addr) 
			  load_addr = elf_ppnt->p_vaddr - elf_ppnt->p_offset;
			k = elf_ppnt->p_vaddr;
			if(k > start_code) start_code = k;
			k = elf_ppnt->p_vaddr + elf_ppnt->p_filesz;
			if(k > elf_bss) elf_bss = k;
			if((elf_ppnt->p_flags | PROT_WRITE) && end_code <  k)
				end_code = k; 
			if(end_data < k) end_data = k; 
			k = elf_ppnt->p_vaddr + elf_ppnt->p_memsz;
			if(k > elf_brk) elf_brk = k;		     
		      };
		elf_ppnt++;
	};
	//set_fs(old_fs);
	
	//kfree(elf_phdata);
	
	//if(!elf_interpreter) sys_close(elf_exec_fileno);
	//current->elf_executable = 1;
	//current->executable = bprm->inode;
	//bprm->inode->i_count++;
#ifdef LOW_ELF_STACK
	current->start_stack = p = elf_stack - 4;
#endif
	bprm->p -= MAX_ARG_PAGES*PAGE_SIZE;
	bprm->p = (unsigned long) 
	  create_elf_tables((char *)bprm->p,
			bprm->argc,
			bprm->envc,
			(interpreter_type == INTERPRETER_ELF ? &elf_ex : NULL),
			load_addr,    
			(interpreter_type == INTERPRETER_AOUT ? 0 : 1));
	/*if(interpreter_type == INTERPRETER_AOUT)
	  current->arg_start += strlen(passed_fileno) + 1;*/
	/*current->start_brk = current->brk = elf_brk;
	current->end_code = end_code;
	current->start_code = start_code;
	current->end_data = end_data;
	current->start_stack = bprm->p;*/
	//current->suid = current->euid = bprm->e_uid;
	//current->sgid = current->egid = bprm->e_gid;

	/* Calling sys_brk effectively mmaps the pages that we need for the bss and break
	   sections */
	//current->brk = (elf_bss + 0xfff) & 0xfffff000;
	sys_brk((elf_brk + 0xfff) & 0xfffff000);

	padzero(elf_bss);

	/* Why this, you ask???  Well SVr4 maps page 0 as read-only,
	   and some applications "depend" upon this behavior.
	   Since we do not have the power to recompile these, we
	   emulate the SVr4 behavior.  Sigh.  */
	error = do_mmap(NULL, 0, 4096, PROT_READ | PROT_EXEC,
			MAP_FIXED | MAP_PRIVATE, 0);

	regs->eip = elf_entry;		/* eip, magic happens :-) */
	regs->esp = bprm->p;			/* stack pointer */
	/*if (current->flags & PF_PTRACED)
		send_sig(SIGTRAP, current, 0);*/
	return 0;
}