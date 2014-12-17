int mem_mark_init();
int set_mem_mark(uint32_t vaddr, uint32_t size, uint64_t mark_bitmap);
uint64_t check_mem_mark(uint32_t vaddr, uint32_t size);
int mem_mark_cleanup();
