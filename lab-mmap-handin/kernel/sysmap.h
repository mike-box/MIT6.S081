#define MAX_ENTRY_SIZE 16

int map_vma_init(struct vmaEntry *vma);
int map_vma_copy(struct vmaEntry *src, struct vmaEntry *dst);
int map_vma_query(uint64 va);
int map_vma_remove(int vma_offset);
int map_readfile(int vma_offset, int offset, uint64 addr, int length);
uint64 sys_mmap(void);
uint64 sys_munmap(void);

