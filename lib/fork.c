// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	
	// if(va==0xeebfe000)
	// 	cprintf("PDE %x, PTE %x, PDE ADDR %x, PTE ADDR %x\n",PDE_USER(va),PTE_USER(va),
	// 	&PDE_USER(va),&PTE_USER(va));

	if((err & FEC_WR) == 0){
		panic("Page fault caused not by write, addr %x\n",addr);
	}
	
	//Access page table to see the permission
	uintptr_t va=(uintptr_t)addr;

	//Round addr and va to page aligned
	va=ROUNDDOWN(va,PGSIZE);
	addr=(void*)va;

	if((PDE_USER(va)&PTE_P) == 0 || (PTE_USER(va)&PTE_P) == 0){
		panic("No page table at %x, eip %x\n",va,utf->utf_eip);
	}

	pte_t pte=PTE_USER(va);
	if((pte & PTE_COW) == 0){
		panic("Try to write a non-COW page, addr %x\n",va);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	//Alloc a new page at PFTEMP
	if((r=sys_page_alloc(sys_getenvid(),(void*)PFTEMP,
	PTE_P|PTE_U|PTE_W)) < 0){
		panic("Failed to alloc temp page, %e\n",r);
	}

	//Copy content
	memcpy((void*)PFTEMP,addr,PGSIZE);

	//Map temp page to addr
	if((r=sys_page_map(sys_getenvid(),(void*)PFTEMP,
	sys_getenvid(),addr,PTE_P|PTE_U|PTE_W))<0){
		panic("Failed to map temp page to addr, %e\n",r);
	}

	//Unmap temp page
	if((r=sys_page_unmap(sys_getenvid(),(void*)PFTEMP))<0){
		panic("Failed to unmap temp page, %e\n",r);
	}
}



//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	bool flag=false;
	// LAB 4: Your code here.
	for(unsigned int i=0;i<pn;i++){
		uintptr_t va=i*PGSIZE;

		//Skip exception stack
		if(va == UXSTACKTOP - PGSIZE){
			continue;
		}

		bool write=true,cow=true;
		//Access page table to see the permission

		if((PDE_USER(va)&PTE_P) == 0){
			continue;
		}

		if((PTE_USER(va)&PTE_P) == 0){
			continue;
		}

		pde_t pde=PDE_USER(va);
		pte_t pte=PTE_USER(va);

		if(pte & PTE_SHARE){
			//Just copy pte
			if((r=sys_page_map(thisenv->env_id,(void*)va,
			envid,(void*)va,pte&0xFFF&PTE_SYSCALL)) < 0){
				return r;
			}
			continue;
		}

		write=(pte & PTE_W) | (pde & PTE_W);
		cow=(pte & PTE_COW) | (pde & PTE_COW);

		int child_perm=PTE_U|PTE_P;
		if(write || cow){
			child_perm |= PTE_COW;
		}

		if((r=sys_page_map(thisenv->env_id,(void*)va,
		envid,(void*)va,child_perm)) <0 ){
			return r;
		}

		//Remap src page cow
		if(write || cow){
			//FIXME: when parent page entry has more perm bits, it will cause them lost....
			if((r=sys_page_map(envid,(void*)va,
			thisenv->env_id,(void*)va,child_perm))<0){
				return r;
			}
		}
	}

	return 0;
}

// Assembly language pgfault entrypoint defined in lib/pfentry.S.
extern void _pgfault_upcall(void);

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;

	set_pgfault_handler(pgfault);

	envid_t envid=sys_exofork();
	if(envid>0){
		//Parent

		//Map all pages below UTOP
		if((r=duppage(envid,UTOP/PGSIZE))<0){
			return r;
		}

		//Allocate exception stack for child
		if((r=sys_page_alloc(envid,(void*)(UXSTACKTOP-PGSIZE),PTE_U|PTE_W|PTE_P)) < 0){
			return r;
		}

		//Setup page fault entry point for child
		if((r=sys_env_set_pgfault_upcall(envid,_pgfault_upcall)) < 0 ){
			return r;
		} 

		//Mark child runnable
		sys_env_set_status(envid,ENV_RUNNABLE);
	}else if(envid==0){
		//Child

		//Reset current environment
		envid_t child_envid=sys_getenvid();
		thisenv =((struct Env*)envs)+ENVX(child_envid);
	}else{
		return envid;
	}

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
