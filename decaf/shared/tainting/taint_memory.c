#include "qemu-common.h"
#include "DECAF_main.h"
#include <string.h> // For memset()
#include "tcg.h"
#include "taint_memory.h"
#include "monitor.h" // For default_mon
#include "DECAF_callback_common.h"
#include "shared/DECAF_callback_to_QEMU.h"

#ifdef CONFIG_TCG_TAINT

#ifndef min
#define min(a, b) ({\
		typeof(a) _a = a;\
		typeof(b) _b = b;\
		_a < _b ? _a : _b; })
#endif

/* Track whether the taint tracking system is enabled or not */
int taint_tracking_enabled = 0;
int taint_nic_enabled = 0;
int taint_pointers_enabled = 0;
int taint_load_pointers_enabled = 0;
int taint_store_pointers_enabled = 0;

/* Root node for holding memory taint information */
tbitpage_middle_t **taint_memory_page_table = NULL;
static uint32_t taint_memory_page_table_root_size = 0;
uint32_t middle_nodes_in_use = 0;
uint32_t leaf_nodes_in_use = 0;

tbitpage_leaf_pool_t leaf_pool;
tbitpage_middle_pool_t middle_pool;
const uint32_t LEAF_ADDRESS_MASK = (2 << BITPAGE_LEAF_BITS) - 1;
const uint32_t MIDDLE_ADDRESS_MASK = (2 << BITPAGE_MIDDLE_BITS) - 1;

static inline tbitpage_leaf_t *read_leaf_node_i32(uint32_t address) {
  unsigned int middle_node_index = address >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
  unsigned int leaf_node_index = (address >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
  // Check for out of range physical address
  if (address >= ram_size)
    return NULL;

  if (taint_memory_page_table[middle_node_index])
    return taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];

  return NULL;
}

static inline void return_leaf_node_to_pool(tbitpage_leaf_t *node) {
  if (leaf_pool.next_available_node != 0) {
    memset((void *)node, 0, sizeof(tbitpage_leaf_t));
    leaf_pool.next_available_node -= 1;
    leaf_pool.pool[leaf_pool.next_available_node] = node;
  } else
    g_free(node);
  leaf_nodes_in_use--;
}

static inline void return_middle_node_to_pool(tbitpage_middle_t *node) {
  if (middle_pool.next_available_node > 0) {
    memset((void *)node, 0, sizeof(tbitpage_middle_t));
    middle_pool.next_available_node -= 1;
    middle_pool.pool[middle_pool.next_available_node] = node;
  } else
    g_free(node);
  middle_nodes_in_use--;
}

static inline tbitpage_leaf_t *fetch_leaf_node_from_pool(void) {
  /* If the pool is empty, refill the pool */
  if (leaf_pool.next_available_node >= BITPAGE_LEAF_POOL_SIZE) allocate_leaf_pool();
  leaf_nodes_in_use++;
  return leaf_pool.pool[leaf_pool.next_available_node++];
}

static inline tbitpage_middle_t *fetch_middle_node_from_pool(void) {
  /* If the pool is empty, refill the pool */
  if (middle_pool.next_available_node >= BITPAGE_MIDDLE_POOL_SIZE) allocate_middle_pool();
  middle_nodes_in_use++;
  return middle_pool.pool[middle_pool.next_available_node++];
}

static inline tbitpage_leaf_t *taint_st_general_i32(const ram_addr_t address, gva_t vaddr, const uint32_t taint)
{
    unsigned int middle_node_index = address >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
    unsigned int leaf_node_index = (address >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
    tbitpage_leaf_t *leaf_node;

    /* Does a middle node exist for this address? */
    if (taint_memory_page_table[middle_node_index]) {
        /* Does a leaf node exist for this address? */
        if (!taint_memory_page_table[middle_node_index]->leaf[leaf_node_index]) {
            if (!taint)
                return NULL;

            /* Pull leaf node from pool and put taint in it */
            leaf_node = fetch_leaf_node_from_pool();
            taint_memory_page_table[middle_node_index]->leaf[leaf_node_index] = leaf_node;
            /* Now we are writing a taint into a newly allocated leaf node. We should flush the TLB entry,
            If vaddr is 0, it means this memory write comes from an IO device. We will need to flush the entire TLB.
            */
            //FIXME: for a multicore system, we should actually flush TLB in all CPUs.
            //printf("taint_st_general_i32: tlb_flush vaddr=%0x taint=%x\n", vaddr, taint);
            if (vaddr)
                tlb_flush_page(cpu_single_env, vaddr);
            else
                tlb_flush(cpu_single_env, 1); //TODO: a more efficient solution is just to flush the entry given a physical address.
        } else {
            leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
        }
        /* Is there no middle node and no taint to add? */
    } else if (!taint)
        return NULL;
    /* Pull middle node from pool */
    else {
        if (!taint)
            return NULL;

        leaf_node = fetch_leaf_node_from_pool();
        taint_memory_page_table[middle_node_index] = fetch_middle_node_from_pool();
        taint_memory_page_table[middle_node_index]->leaf[leaf_node_index] = leaf_node;
        /* Now we are writing a taint into a newly allocated leaf node. We should flush the TLB entry,
        so the related entry will be marked as io_mem_taint (or io_mem_notdirty). The virtual address is stored in env->mem_io_vaddr,
        because all tainted memory writes either go through io_mem_write (i.e., io_mem_taint or io_mem_notdirty).
        FIXME: what about DMA? There will be no related virtual address.
        */
        //printf("taint_st_general_i32: tlb_flush vaddr=%0x taint=%x\n", vaddr, taint);
        if (vaddr)
            tlb_flush_page(cpu_single_env, vaddr);
        else
            tlb_flush(cpu_single_env, 1); //TODO: a more efficient solution is just to flush the entry given a physical address.
    }
    return leaf_node;
}

void allocate_leaf_pool(void) {
  int i;
  for (i=0; i < BITPAGE_LEAF_POOL_SIZE; i++)
    leaf_pool.pool[i] = (tbitpage_leaf_t *)g_malloc0(sizeof(tbitpage_leaf_t));
  leaf_pool.next_available_node = 0;
}

void allocate_middle_pool(void) {
  int i;
  for (i=0; i < BITPAGE_MIDDLE_POOL_SIZE; i++)
    middle_pool.pool[i] = (tbitpage_middle_t *)g_malloc0(sizeof(tbitpage_middle_t));
  middle_pool.next_available_node = 0;
}

static void free_pools(void) {
  int i;
  for (i=leaf_pool.next_available_node; i < BITPAGE_LEAF_POOL_SIZE; i++)
    if (leaf_pool.pool[i] != NULL) {
      g_free(leaf_pool.pool[i]);
      leaf_pool.pool[i] = NULL;
    }
  for (i=middle_pool.next_available_node; i < BITPAGE_MIDDLE_POOL_SIZE; i++)
    if (middle_pool.pool[i] != NULL) {
      g_free(middle_pool.pool[i]);
      middle_pool.pool[i] = NULL;
    }
  leaf_pool.next_available_node = 0;
  middle_pool.next_available_node = 0;
}

static void allocate_taint_memory_page_table(void) {
  if (taint_memory_page_table) return; // AWH - Don't allocate if one exists
  taint_memory_page_table_root_size = ram_size >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
  taint_memory_page_table = (tbitpage_middle_t **)
    g_malloc0(taint_memory_page_table_root_size * sizeof(void*));
  allocate_leaf_pool();
  allocate_middle_pool();
  middle_nodes_in_use = 0;
  leaf_nodes_in_use = 0;
}

void garbage_collect_taint(int flag) {
  uint32_t middle_index;
  uint32_t leaf_index;
  uint32_t i, free_leaf, free_middle;
  tbitpage_middle_t *middle_node = NULL;
  tbitpage_leaf_t *leaf_node = NULL;

  static uint32_t counter = 0;

  if (!taint_memory_page_table || !taint_tracking_enabled) return;

  if (!flag && (counter < 4 * 1024)) { counter++; return; }
  counter = 0;
  for (middle_index = 0; middle_index < taint_memory_page_table_root_size; middle_index++) {
    middle_node = taint_memory_page_table[middle_index];
    if (middle_node) {
      free_middle = 1;
      for (leaf_index = 0; leaf_index < (2 << BITPAGE_MIDDLE_BITS); leaf_index++) {
        leaf_node = middle_node->leaf[leaf_index];
        if (leaf_node) {
          free_leaf = 1;
          // Take the byte array elements of the leaf node four at a time
          for (i = 0; i < (2 << (BITPAGE_LEAF_BITS-2)); i++) {
            if ( *(((uint32_t *)leaf_node->bitmap) + i) ) {
              free_leaf = 0;
              free_middle = 0;
            }
          }
          if (free_leaf) {
            return_leaf_node_to_pool(leaf_node);
            middle_node->leaf[leaf_index] = NULL;
          }
        } // if leaf_node
      } // End for loop

      if (free_middle) {
        return_middle_node_to_pool(middle_node);
        taint_memory_page_table[middle_index] = NULL;
      }
    } // if middle_node
  } // End for loop
}

static void empty_taint_memory_page_table(void) {
  uint32_t middle_index;
  uint32_t leaf_index;
  tbitpage_middle_t *middle_node = NULL;
  tbitpage_leaf_t *leaf_node = NULL;

  if (!taint_memory_page_table) return; /* If there's no root, exit */
  for (middle_index = 0; middle_index < taint_memory_page_table_root_size; middle_index++) {
    middle_node = taint_memory_page_table[middle_index];
    if (middle_node) {
      for (leaf_index = 0; leaf_index < (2 << BITPAGE_MIDDLE_BITS); leaf_index++) {
        leaf_node = middle_node->leaf[leaf_index];
        if (leaf_node) {
          g_free(leaf_node);
          leaf_node = NULL;
        }
      }
    }
    g_free(middle_node);
    middle_node = NULL;
  }
}

/* This deallocates all of the nodes in the tree, including the root */
static void free_taint_memory_page_table(void) {
  empty_taint_memory_page_table();
  g_free(taint_memory_page_table);
  taint_memory_page_table = NULL;
  free_pools();
}

int is_physial_page_tainted(ram_addr_t addr)
{
    unsigned int middle_node_index;
    unsigned int leaf_node_index;
    tbitpage_leaf_t *leaf_node = NULL;

    if (!taint_memory_page_table || addr >= ram_size)
        return 0;

    middle_node_index = addr >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
    leaf_node_index = (addr >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;

    if (!taint_memory_page_table[middle_node_index])
        return 0;

    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
    return (leaf_node != NULL);
}


void REGPARM __taint_ldb_raw_paddr(ram_addr_t addr,gva_t vaddr)
{
	unsigned int middle_node_index;
	unsigned int leaf_node_index;
	tbitpage_leaf_t *leaf_node = NULL;
	cpu_single_env->tempidx = 0;
	cpu_single_env->tempidx2 = 0;

	if (!taint_memory_page_table || addr >= ram_size) return;

	middle_node_index = addr >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
	leaf_node_index = (addr >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;

	if (!taint_memory_page_table[middle_node_index])
        return;

    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
	if (!leaf_node)
        return;

    cpu_single_env->tempidx = (*(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));

    if (cpu_single_env->tempidx && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB)){
    	helper_DECAF_invoke_read_taint_mem(vaddr,addr,1,(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
    }
}


void REGPARM __taint_ldw_raw_paddr(ram_addr_t addr,gva_t vaddr)
{
	unsigned int middle_node_index;
	unsigned int leaf_node_index;
	tbitpage_leaf_t *leaf_node = NULL;

	cpu_single_env->tempidx = 0;
	cpu_single_env->tempidx2 = 0;

	if (!taint_memory_page_table || addr >= ram_size) return;

    middle_node_index = addr >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
	leaf_node_index = (addr >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
	if (!taint_memory_page_table[middle_node_index])
        return;

	leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];

	if (!leaf_node)
        return;

    cpu_single_env->tempidx =  (*(uint16_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	if (cpu_single_env->tempidx && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB)) {
		helper_DECAF_invoke_read_taint_mem(vaddr,addr,2,(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
    }
}

void REGPARM __taint_ldl_raw_paddr(ram_addr_t addr,gva_t vaddr)
{
	unsigned int middle_node_index;
	unsigned int leaf_node_index;
	tbitpage_leaf_t *leaf_node = NULL;

	cpu_single_env->tempidx = 0;
	cpu_single_env->tempidx2 = 0;

	if (!taint_memory_page_table || addr >= ram_size) return;

    middle_node_index = addr >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
	leaf_node_index = (addr >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;

    if (!taint_memory_page_table[middle_node_index])
        return;

    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
    if (!leaf_node)
        return;

    cpu_single_env->tempidx = (*(uint32_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	if (cpu_single_env->tempidx && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB)) {
		helper_DECAF_invoke_read_taint_mem(vaddr,addr,4,(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	}
}


void REGPARM __taint_ldq_raw_paddr(ram_addr_t addr,gva_t vaddr)
{
	unsigned int middle_node_index;
	unsigned int leaf_node_index;
	tbitpage_leaf_t *leaf_node = NULL;
	cpu_single_env->tempidx = 0;
	cpu_single_env->tempidx2 = 0;
	uint32_t taint_temp[2];

	if (!taint_memory_page_table || addr >= ram_size) return;

	middle_node_index = addr >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
	leaf_node_index = (addr >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
	if (!taint_memory_page_table[middle_node_index])
        return;

    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
    if (!leaf_node)
        return;

    //FIXME: need to handle different endianness between guest and host.
    //Right now, we only assume little endian for memory on both
#if TARGET_LONG_BITS == 64
    cpu_single_env->tempidx = (*(uint64_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
#else
    cpu_single_env->tempidx = (*(uint32_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
    cpu_single_env->tempidx2 = (*(uint32_t *)(leaf_node->bitmap + ((addr+4) & LEAF_ADDRESS_MASK)));
#endif

    if ((cpu_single_env->tempidx || cpu_single_env->tempidx2) && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB)) {
	   taint_temp[0] = cpu_single_env->tempidx;
	   taint_temp[1] = cpu_single_env->tempidx2;
	   helper_DECAF_invoke_read_taint_mem(vaddr, addr, 8, (uint8_t *) taint_temp);
	}
}

void REGPARM __taint_ldb_raw(void *p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_ldb_raw_paddr(addr,vaddr);

}

void REGPARM __taint_ldw_raw(void *p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_ldw_raw_paddr(addr,vaddr);
}

void REGPARM __taint_ldl_raw(void *p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_ldl_raw_paddr(addr,vaddr);
}

void REGPARM __taint_ldq_raw(void *p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_ldq_raw_paddr(addr,vaddr);
}

void REGPARM __taint_stb_raw_paddr(ram_addr_t addr, gva_t vaddr) {
	if (!taint_memory_page_table || addr >= ram_size)
		return;

	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
	uint16_t before, after;
	char changed = 0;

	tbitpage_leaf_t *leaf_node = taint_st_general_i32(addr, vaddr,
			cpu_single_env->tempidx & 0xFF);
	if (leaf_node) {
		before = *(uint8_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
		*(uint8_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)) =
				cpu_single_env->tempidx & 0xFF;
		after = *(uint8_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
    if ((before != after) || (cpu_single_env->tempidx & 0xFF)) changed = 1;
  }
	if ( changed && DECAF_is_callback_needed( DECAF_WRITE_TAINTMEM_CB) )
		helper_DECAF_invoke_write_taint_mem(vaddr,addr,1,(uint8_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	return;
}

void REGPARM __taint_stw_raw_paddr(ram_addr_t addr,gva_t vaddr) {
	if (!taint_memory_page_table || addr >= ram_size)
		return;

	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
  uint16_t before, after;
  char changed = 0;

	tbitpage_leaf_t *leaf_node = taint_st_general_i32(addr, vaddr,
			cpu_single_env->tempidx & 0xFFFF);
	if (leaf_node) {
		before = *(uint16_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
		*(uint16_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)) =
				(uint16_t) cpu_single_env->tempidx & 0xFFFF;
    after = *(uint16_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
    if ((before != after) || (cpu_single_env->tempidx & 0xFFFF)) changed = 1;
	}
	if ( changed && DECAF_is_callback_needed( DECAF_WRITE_TAINTMEM_CB) ) {
		helper_DECAF_invoke_write_taint_mem(vaddr,addr,2,(uint8_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
  }
	return;
}

void REGPARM __taint_stl_raw_paddr(ram_addr_t addr,gva_t vaddr) {
	if (!taint_memory_page_table || addr >= ram_size)
		return;

	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
	uint16_t before, after;
	char changed = 0;

	tbitpage_leaf_t *leaf_node = taint_st_general_i32(addr, vaddr,
			cpu_single_env->tempidx & 0xFFFFFFFF);
	if (leaf_node) {
		before = *(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
		*(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)) =
				cpu_single_env->tempidx & 0xFFFFFFFF;
		after = *(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
		if ((before != after) || (cpu_single_env->tempidx & 0xFFFFFFFF)) changed = 1;
	}
	if ( changed && DECAF_is_callback_needed( DECAF_WRITE_TAINTMEM_CB) )
		helper_DECAF_invoke_write_taint_mem(vaddr,addr,4,(uint8_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	return;
}

void REGPARM __taint_stq_raw_paddr(ram_addr_t addr, gva_t vaddr) {
	if (!taint_memory_page_table || addr >= ram_size)
		return;

	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
	//uint16_t before, after;
	//char changed = 0;

	tbitpage_leaf_t *leaf_node = NULL, *leaf_node2 = NULL;
	uint32_t taint_temp[2];

	/* AWH - FIXME - BUG - 64-bit stores aren't working right, workaround */
	cpu_single_env->tempidx = 0;
	cpu_single_env->tempidx2 = 0;


    //FIXME: endianness
#if TARGET_LONG_BITS == 64
    leaf_node = taint_st_general_i32(addr, vaddr, cpu_single_env->tempidx);
    if (leaf_node) {
        *(uint64_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)) = cpu_single_env->tempidx;
    }
#else
	leaf_node = taint_st_general_i32(addr, vaddr, cpu_single_env->tempidx || cpu_single_env->tempidx2);
	if (leaf_node) {
		*(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)) =
				cpu_single_env->tempidx;
		*(uint32_t *) (leaf_node2->bitmap + ((addr+4) & LEAF_ADDRESS_MASK)) =
				cpu_single_env->tempidx2;
	}
#endif /* TCG_TARGET_REG_BITS check */

	if ((cpu_single_env->tempidx || cpu_single_env->tempidx2) && DECAF_is_callback_needed (DECAF_WRITE_TAINTMEM_CB) )
	{
		taint_temp[0] = cpu_single_env->tempidx;
		taint_temp[1] = cpu_single_env->tempidx2;
		helper_DECAF_invoke_write_taint_mem(vaddr, addr, 8, (uint8_t *)taint_temp);
	}
}

void REGPARM __taint_stb_raw(void * p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_stb_raw_paddr(addr, vaddr);
}

void REGPARM __taint_stw_raw(void *p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_stw_raw_paddr(addr, vaddr);
}

void REGPARM __taint_stl_raw(void *p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_stl_raw_paddr(addr, vaddr);
}

void REGPARM __taint_stq_raw(void *p, gva_t vaddr) {
	ram_addr_t addr = qemu_ram_addr_from_host_nofail((void*)p);
	__taint_stq_raw_paddr(addr, vaddr);
}

/*
addr: physical addr
size: memory size expected to taint
taint: pointer to a buffer containing taint information. "size" is also the size
of this buffer.
*/
void REGPARM taint_mem(ram_addr_t addr, int size, uint8_t *taint)
{
	uint32_t i, offset, len = 0;
    tbitpage_leaf_t *leaf_node = NULL;
    int is_tainted;
    uint8_t zero_mem[2 << BITPAGE_LEAF_BITS];

    bzero(zero_mem, sizeof(zero_mem)); //TODO: would be nice to zero it only once.
    for (i=0; i<size; i+=len) {
		offset = (addr + i) & LEAF_ADDRESS_MASK;
		len = min( (2<<BITPAGE_LEAF_BITS) - offset, size - i);
        is_tainted = (memcmp(taint+i, zero_mem, len)!=0);

        //the name of this function is a little misleading.
        //What we want is to get a leaf_node based on the address.
        //We set vaddr as zero, so it may flush the entire TLB if a new tainted page is found.
        leaf_node = taint_st_general_i32(addr+i, 0, is_tainted);
		if (leaf_node) {
			memcpy(&leaf_node->bitmap[offset], taint+i, len);
		}
    }
}


void REGPARM taint_mem_check(ram_addr_t addr, uint32_t size, uint8_t * taint)
{
	tbitpage_leaf_t *leaf_node = NULL;
    uint32_t i, offset, len=0;

  	bzero(taint, size);
    for (i=0; i<size; i+=len) {
        offset = (addr + i) & LEAF_ADDRESS_MASK;
        len = min((2<<BITPAGE_LEAF_BITS) - offset, size - i);
        leaf_node = read_leaf_node_i32(addr + i);
        if(leaf_node) {
            memcpy(taint+i, &leaf_node->bitmap[offset], len);
        }
    }
}

uint32_t calc_tainted_bytes(void){
	uint32_t tainted_bytes, i;
	uint32_t leaf_index;
	uint32_t middle_index;
	tbitpage_middle_t *middle_node = NULL;
	tbitpage_leaf_t *leaf_node = NULL;

	if (!taint_memory_page_table)
		return 0;
	tainted_bytes = 0;
	for (middle_index = 0; middle_index < taint_memory_page_table_root_size;
			middle_index++) {
		middle_node = taint_memory_page_table[middle_index];
		if (middle_node) {
			for (leaf_index = 0; leaf_index < (2 << BITPAGE_MIDDLE_BITS);
					leaf_index++) {
				leaf_node = middle_node->leaf[leaf_index];
				if (leaf_node) {
					for (i = 0; i < (2 << BITPAGE_LEAF_BITS); i++) {
						if (leaf_node->bitmap[i])
							tainted_bytes++;
					}
				}
			}
		}
	}
	return tainted_bytes;
}
/* Console control commands */
void do_enable_tainting_internal(void) {
  if (!taint_tracking_enabled) {
    CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
    DECAF_stop_vm();
    tb_flush(env);
    allocate_taint_memory_page_table();
    taint_tracking_enabled = 1;
    DECAF_start_vm();
  }
}

void do_disable_tainting_internal(void) {
  if (taint_tracking_enabled) {
    CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

    DECAF_stop_vm();
    tb_flush(env);
    free_taint_memory_page_table();
    taint_tracking_enabled = 0;
    DECAF_start_vm();
  }
}

int do_enable_taint_nic_internal(void) {
  if (!taint_nic_enabled) {
    DECAF_stop_vm();
    taint_nic_enabled = 1;
    DECAF_start_vm();
  }
  return 0;
}

int do_disable_taint_nic_internal(void) {
  if (taint_nic_enabled) {
    DECAF_stop_vm();
    taint_nic_enabled = 0;
    DECAF_start_vm();
  }
  return 0;
}

int do_enable_tainting(Monitor *mon, const QDict *qdict, QObject **ret_data) {
  if (!taint_tracking_enabled) {
    do_enable_tainting_internal();
    monitor_printf(default_mon,  "Taint tracking is now enabled (fresh taint data generated)\n");
  } else
    monitor_printf(default_mon, "Taint tracking is already enabled\n");
  return 0;
}

int do_disable_tainting(Monitor *mon, const QDict *qdict, QObject **ret_data) {
  if (taint_tracking_enabled) {
    do_disable_tainting_internal();
    monitor_printf(default_mon, "Taint tracking is now disabled (all taint data discarded)\n");
  } else
    monitor_printf(default_mon, "Taint tracking is already disabled\n");
  return 0;
}

int do_taint_nic_off(Monitor *mon, const QDict *qdict, QObject **ret_data) {
  if (taint_tracking_enabled && taint_nic_enabled) {
    do_disable_taint_nic_internal();
    monitor_printf(default_mon, "NIC tainting is now disabled.\n");
  } else if (taint_tracking_enabled) {
    monitor_printf(default_mon, "Ignored, NIC tainting was already disabled.\n");
  } else
    monitor_printf(default_mon, "Ignored, taint tracking is disabled.  Use the 'enable_tainting' command to enable tainting first.\n");
  return 0;
}

int do_taint_nic_on(Monitor *mon, const QDict *qdict, QObject **ret_data) {
  if (taint_tracking_enabled && !taint_nic_enabled) {
    do_enable_taint_nic_internal();
    monitor_printf(default_mon, "NIC tainting is now enabled.\n");
  } else if (taint_tracking_enabled) {
    monitor_printf(default_mon, "Ignored, NIC tainting was already enabled.\n");
  } else
    monitor_printf(default_mon, "Ignored, taint tracking is disabled.  Use the 'enable_tainting' command to enable tainting first.\n");
  return 0;
}
int do_tainted_bytes(Monitor *mon,const QDict *qdict,QObject **ret_data){
  uint32_t tainted_bytes;
  if(!taint_tracking_enabled)
    monitor_printf(default_mon,"Taint tracking is disabled,no statistics available\n");
  else{
     tainted_bytes=calc_tainted_bytes();
     monitor_printf(default_mon,"Tainted memory: %d bytes\n",tainted_bytes);
  }
  return 0;
}
int do_taint_mem_usage(Monitor *mon, const QDict *qdict, QObject **ret_data) {
  if (!taint_tracking_enabled)
    monitor_printf(default_mon, "Taint tracking is disabled, no statistics available\n");
  else
    monitor_printf(default_mon, "%uM RAM: %d mid nodes, %d leaf nodes, %d/%d mid pool, %d/%d leaf pool\n",
      ((unsigned int)(ram_size)) >> 20, middle_nodes_in_use, leaf_nodes_in_use,
      BITPAGE_MIDDLE_POOL_SIZE - middle_pool.next_available_node, BITPAGE_MIDDLE_POOL_SIZE,
      BITPAGE_LEAF_POOL_SIZE - leaf_pool.next_available_node, BITPAGE_LEAF_POOL_SIZE);
  return 0;
}

int do_garbage_collect_taint(Monitor *mon, const QDict *qdict, QObject **ret_data) {
  if (!taint_tracking_enabled)
    monitor_printf(default_mon, "Ignored, taint tracking is disabled\n");
  else
  {
    int prior_middle, prior_leaf/*, present_middle, present_leaf*/;
    prior_middle = middle_nodes_in_use;
    prior_leaf = leaf_nodes_in_use;

    garbage_collect_taint(1);

    monitor_printf(default_mon, "Garbage Collector: Removed %d mid nodes, %d leaf nodes\n", prior_middle - middle_nodes_in_use, prior_leaf - leaf_nodes_in_use);
  }
  return 0;

}

int do_taint_pointers(Monitor *mon, const QDict *qdict, QObject **ret_data)
{
  if (!taint_tracking_enabled)
    monitor_printf(default_mon, "Ignored, taint tracking is disabled\n");
  else {
    CPUState *env;
    DECAF_stop_vm();
    env = cpu_single_env ? cpu_single_env : first_cpu;
    taint_load_pointers_enabled = qdict_get_bool(qdict, "load");
    taint_store_pointers_enabled = qdict_get_bool(qdict, "store");
    tb_flush(env);
    DECAF_start_vm();
    monitor_printf(default_mon, "Tainting of pointers changed -> Load: %s, Store: %s\n", taint_load_pointers_enabled ? "ON " : "OFF", taint_store_pointers_enabled ? "ON " : "OFF");
  }
  return 0;
}

#endif /* CONFIG_TCG_TAINT */
