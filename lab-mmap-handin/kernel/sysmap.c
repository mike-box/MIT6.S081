#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "sysmap.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

int map_readfile(int vma_offset, int offset, uint64 addr, int length) 
{
	struct proc * p = myproc();
	struct file *pf = p->vma[vma_offset].pf;
	int ret = 0;
	ilock(pf->ip);
	ret = readi(pf->ip, 0, addr, offset, length);
	iunlock(pf->ip);
	return ret;
}

int map_writefile(struct file *pf, int offset, uint64 addr, int length) 
{
	int ret = 0;
	begin_op();
	ilock(pf->ip);
	ret = writei(pf->ip, 1, addr, offset, length);
	ilock(pf->ip);
	end_op();
	return ret;
}


int map_vma_init(struct vmaEntry *vma) {
   for (int i = 0; i < MAX_ENTRY_SIZE; i++) {
	  vma[i].valid = 0;
   }
   return 0;
}

int map_vma_copy(struct vmaEntry *src, struct vmaEntry *dst) {
   for (int i = 0; i < MAX_ENTRY_SIZE; i++) {
		if (src[i].valid) {
		   dst[i].addr = src[i].addr;
		   dst[i].length = src[i].length;
		   dst[i].permissions = src[i].permissions;
		   dst[i].flag = src[i].flag;
		   dst[i].pf = src[i].pf;
		   dst[i].offset = src[i].offset;
		   dst[i].valid = 1;
		   dst[i].pf = filedup(dst[i].pf);
		} else {
		   dst[i].valid = 0;
		}
	 }
   return 0;
}


int map_vma_query(uint64 va) {
   struct proc *p = myproc();
   for (int i = 0; i < MAX_ENTRY_SIZE; i++) {
   	   if (p->vma[i].valid) {
		   int offset = va - p->vma[i].addr;
		   if (offset >= 0 && offset < p->vma[i].length) {
				return i;
		   }
   	   }
   }
   return -1;
}

int map_vma_add(uint64 addr, struct file *f, int len, int prot, int flag, int offset) {
  struct proc * p = myproc();
  
  /* record the vma of the current process */
  f->ref++;
  for (int i = 0; i < MAX_ENTRY_SIZE; i++) {
  	if (!p->vma[i].valid) {
		p->vma[i].valid = 1;
		p->vma[i].addr = addr;
		p->vma[i].length = len;
		p->vma[i].permissions = prot;
		p->vma[i].flag = flag;
		p->vma[i].pf = f;
		p->vma[i].offset = offset & ~(PGSIZE - 1);
		break;
	}
  }
  return 0;
}


int map_vma_remove(int vma_offset) {
	struct proc *p = myproc();
	
	/* remove all the pages from process */
	if ((p->vma[vma_offset].permissions & PROT_WRITE) && p->vma[vma_offset].flag == MAP_SHARED) {
		filewrite(p->vma[vma_offset].pf, p->vma[vma_offset].addr, p->vma[vma_offset].length);
	}
	uvmunmap(p->pagetable, p->vma[vma_offset].addr, p->vma[vma_offset].length / PGSIZE, 0);
	/* remove all mapped pages */
	p->vma[vma_offset].pf->ref--;
	p->vma[vma_offset].valid = 0;
	return 0;
}

int map_unmap(uint64 addr, int length) 
{
	int vma_offset = map_vma_query(addr);
	if (vma_offset == -1) {
		return -1;
	}
	struct proc *p = myproc();

	/* remove all the pages from process */
	if (p->vma[vma_offset].flag == MAP_SHARED) {
		filewrite(p->vma[vma_offset].pf, p->vma[vma_offset].addr, p->vma[vma_offset].length);
	}
	uvmunmap(p->pagetable, p->vma[vma_offset].addr, length / PGSIZE, 0);
	p->vma[vma_offset].length -= length;
	p->vma[vma_offset].addr += length;
	/* remove all mapped pages */
	if (p->vma[vma_offset].length == 0) {
		p->vma[vma_offset].pf->ref--;
		p->vma[vma_offset].valid = 0;
	}
	return 0;
}


/* map files into process address spaces */
uint64
sys_mmap(void)
{
  uint64 addr;	
  int len;
  int prot;
  int flag;
  int offset;
  int fd;
  struct file *f;

  /* parse syscall args */
  if(argaddr(0, &addr) < 0 || argint(1, &len) < 0 || argint(2, &prot) < 0 || \
  	 argint(3, &flag) < 0 || argint(4, &fd) < 0 || argint(5, &offset) < 0) {
  	 return -1;
  }

  /* fd to file struct */
  if(fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0) {
     return -1;
  }

  /* we check the file readable and writeable */
  if (f->writable == 0 && flag == MAP_SHARED && (prot & PROT_WRITE)) {
	 return -1;
  }
  
  /* we add the map address at the end of virtual address */
  addr = myproc()->sz;
  struct proc * p = myproc();
  if(len > 0){
	if(p->sz + len > MAXVA) {
	  return -1;
	}
	p->sz += len;
  }
  map_vma_add(addr, f, len, prot, flag, offset);
  return addr;
}


/* unmap files into process address spaces */
uint64
sys_munmap(void)
{
	uint64 addr;	
	int len;

	/* parse syscall args */
	if(argaddr(0, &addr) < 0 || argint(1, &len) < 0) {
	 	return -1;
	}
	
	/* we add the map address at the end of virtual address */	
	int vma_offset = map_vma_query(addr);
	if (vma_offset == -1) {
		 return -1;
	}
  
	/* remove all the pages from process */
	map_unmap(addr, len);
	return 0;
}

