/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU GPL, version 3 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
//Removed --Heng
//uncomment this
#ifndef TAINTCHECK_OPT_H_INCLUDED
#define TAINTCHECK_OPT_H_INCLUDED

#include <stdint.h> // AWH
#ifdef CONFIG_TCG_TAINT
#include "taint_memory.h"

#endif /* CONFIG_TCG_TAINT*/
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


#define size_to_mask(size) ((1u << (size*8)) - 1u) //size<=4

#ifdef CONFIG_TCG_TAINT

static inline uint64_t taintcheck_register_check(int regid,int offset,int size,CPUState *env){
	int off = offset*8;
#if defined(TARGET_MIPS)
    return (size < 4) ? (env->active_tc.taint_gpr[regid]>>off)
        &size_to_mask(size):env->active_tc.taint_gpr[regid]>>off;
#else
    return (size < 4) ? (env->taint_regs[regid]>>off)&size_to_mask(size):
    		env->taint_regs[regid]>>off;
#endif /* CONFIG_MIPS */
}
/*
addr: physical addr
size: memory size expected to taint
taint: pointer to a buffer containing taint information. "size" is also the size
of this buffer.
*/
//TODO: Flush TLB when allocate a new leaf node to store taint.
//Question: what is the virtual address?
static inline int taint_mem(uint32_t addr, int size, uint8_t *taint)
{
        uint32_t middle_node_index;
        uint32_t leaf_node_index;
        tbitpage_leaf_t *leaf_node = NULL;
        tbitpage_middle_t *middle_node=NULL;
        int i;
        if(!taint_tracking_enabled||!taint_memory_page_table) return 0;
        for(i=0;i<size;i++){
          if(addr+i>=ram_size) return 0;
          middle_node_index=(addr+i)>>(BITPAGE_LEAF_BITS+BITPAGE_MIDDLE_BITS);
          leaf_node_index=((addr+i)>> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
          middle_node=taint_memory_page_table[middle_node_index];
	  if(!middle_node)
	    taint_memory_page_table[middle_node_index]=fetch_middle_node_from_pool();
          leaf_node=taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
          if(!leaf_node){
	    taint_memory_page_table[middle_node_index]->leaf[leaf_node_index]=fetch_leaf_node_from_pool();
	    leaf_node=taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
          }
          leaf_node->bitmap[(addr+i)&LEAF_ADDRESS_MASK]=taint[i];
        }
	return 1;
}


//
//the size should be less or equal to 2*TARGET_PAGE_SIZZE
//taint points to a uint8_t array,which size is defined by second parameter "size"
//after return, the array pointed to by parameter taint stores  the bit wise
// taint status of memory (vaddr,vaddr+size)
//the mapping between taint array  and memory is:
//taint:       taint[0],taint[1],....taint[size-1]
//Mem:          vaddr,vaddr+1,...vaddr+size
//be carefull when check the taint status of specified byte in memory

static inline void taint_mem_check(uint32_t addr, uint32_t size, uint8_t * taint)
{
	tbitpage_leaf_t *leaf_node = NULL;
  // Initialize taint buf
  memset(taint, 0, size);
//	if(!taint_memory_page_table||!taint_tracking_enabled) return 0;
	leaf_node = read_leaf_node_i32(addr);
	if(leaf_node){
          memcpy(taint, (uint8_t*)(leaf_node->bitmap+(addr&LEAF_ADDRESS_MASK)), size);
	 }
}

uint64_t taintcheck_memory_check(uint32_t addr, int size);

int taintcheck_check_virtmem(gva_t vaddr, uint32_t size,uint8_t *taint);

int  taintcheck_taint_virtmem(gva_t vaddr, uint32_t size, uint8_t * taint);

void taintcheck_nic_writebuf(const uint32_t addr, const int size, const uint8_t * taint);

void taintcheck_nic_readbuf(const uint32_t addr, const int size, uint8_t *taint);

void taintcheck_nic_cleanbuf(const uint32_t addr, const int size);

int taintcheck_chk_hdread(const ram_addr_t paddr, const unsigned long vaddr,const int size, const int64_t sect_num, const void *s);

int taintcheck_chk_hdwrite(const ram_addr_t paddr, const unsigned long vaddr,const int size, const int64_t sect_num, const void *s);

int taintcheck_taint_disk(const uint64_t index, const uint32_t taint, const int offset, const int size, const void *bs);

uint32_t taintcheck_disk_check(const uint64_t index, const int offset, const int size, const void *bs);

int taintcheck_init(void);

void taintcheck_cleanup(void);

int taintcheck_chk_hdin(const int size, const int64_t sect_num, const uint32_t offset, const void *s);

int taintcheck_chk_hdout(const int size, const int64_t sect_num, const uint32_t offset, const void *s);

#endif /* CONFIG_TCG_TAINT */
#ifdef __cplusplus
}
#endif // __cplusplus

#endif //TAINTCHECK_OPT_H_INCLUDED
