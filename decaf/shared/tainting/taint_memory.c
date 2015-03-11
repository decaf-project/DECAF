#include "qemu-common.h"
#include "DECAF_main.h"
#include <string.h> // For memset()
#include "tcg.h"
#include "taint_memory.h"
#include "monitor.h" // For default_mon
#include "DECAF_callback_common.h"
#include "shared/DECAF_callback_to_QEMU.h"

#ifdef CONFIG_TCG_TAINT

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
  DECAF_stop_vm();
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
  DECAF_start_vm();
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

	  if (taint_memory_page_table[middle_node_index])
	    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
	  else
	    return;

	  if (leaf_node) {
	    cpu_single_env->tempidx = (*(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	    cpu_single_env->tempidx = cpu_single_env->tempidx & 0xFF;
	//if (cpu_single_env->tempidx) { fprintf(stderr, "__taint_ldb_raw(0x%08x) -> 0x%08x\n", addr, cpu_single_env->tempidx); __asm__ ("int $3"); }
	    if (cpu_single_env->tempidx && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB)){
	    	helper_DECAF_invoke_read_taint_mem(vaddr,addr,1,(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	    }
	  }
	  return;

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
	  if (taint_memory_page_table[middle_node_index])
	    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
	  else
	    return;

	  if (leaf_node) {
	    cpu_single_env->tempidx =  (*(uint16_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	    cpu_single_env->tempidx = cpu_single_env->tempidx & 0xFFFF;
	//if (cpu_single_env->tempidx) { fprintf(stderr, "__taint_ldw_raw(0x%08x) -> 0x%08x\n", addr, cpu_single_env->tempidx); __asm__ ("int $3"); }
		if (cpu_single_env->tempidx && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB)) {
			helper_DECAF_invoke_read_taint_mem(vaddr,addr,2,(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
		}
	  }
	  return;
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
	  if (taint_memory_page_table[middle_node_index])
	    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
	  else
	    return;

	  if (leaf_node) {
	    cpu_single_env->tempidx = (*(uint32_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	#if TCG_TARGET_REG_BITS == 64
	    cpu_single_env->tempidx = cpu_single_env->tempidx & 0xFFFFFFFF;
	#endif /* TCG_TARGET_REG_BITS == 64 */
	//if (cpu_single_env->tempidx) { fprintf(stderr, "__taint_ldl_raw(0x%08x) -> 0x%08x\n", addr, cpu_single_env->tempidx); __asm__ ("int $3"); }
		if (cpu_single_env->tempidx && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB)) {
			helper_DECAF_invoke_read_taint_mem(vaddr,addr,4,(uint8_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
		}

	  }
	  return;
}
void REGPARM __taint_ldq_raw_paddr(ram_addr_t addr,gva_t vaddr)
{
	  unsigned int middle_node_index;
	  unsigned int leaf_node_index;
	  tbitpage_leaf_t *leaf_node = NULL, *leaf_node2 = NULL;
	  cpu_single_env->tempidx = 0;
	  cpu_single_env->tempidx2 = 0;
	  uint32_t taint_temp[2];

	  if (!taint_memory_page_table || addr >= ram_size) return;
	  middle_node_index = addr >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
	  leaf_node_index = (addr >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
	  if (taint_memory_page_table[middle_node_index])
	    leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];

	  if ((addr + 4) >= ram_size) return;
	  middle_node_index = (addr + 4) >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
	  leaf_node_index = ((addr + 4) >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
	  if (taint_memory_page_table[middle_node_index])
	    leaf_node2 = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];

	  if (leaf_node) {
	    cpu_single_env->tempidx = (*(uint32_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)));
	#if TCG_TARGET_REG_BITS == 64
	    cpu_single_env->tempidx = cpu_single_env->tempidx & 0xFFFFFFFF;
	#endif /* TCG_TARGET_REG_BITS == 64 */
	  }
	  if (leaf_node2) {
	#if TCG_TARGET_REG_BITS == 64
	    cpu_single_env->tempidx |= (*(uint32_t *)(leaf_node2->bitmap + ((addr+4) & LEAF_ADDRESS_MASK))) << 32;
	#else
	    cpu_single_env->tempidx2 = (*(uint32_t *)(leaf_node2->bitmap + ((addr+4) & LEAF_ADDRESS_MASK)));
	#endif /* TCG_TARGET_REG_BITS == 64 */
	  }
	// 32 -bit debug
	//if (cpu_single_env->tempidx || cpu_single_env->tempidx2) { fprintf(stderr, "__taint_ldq_raw(0x%08x) -> 0x%08x, 0x%08x\n", addr, cpu_single_env->tempidx, cpu_single_env->tempidx2); __asm__ ("int $3"); }
	  if ((cpu_single_env->tempidx || cpu_single_env->tempidx2) && DECAF_is_callback_needed(DECAF_READ_TAINTMEM_CB))
	  {
		  taint_temp[0] = cpu_single_env->tempidx;
		  taint_temp[1] = cpu_single_env->tempidx2;
		  helper_DECAF_invoke_read_taint_mem(vaddr,addr,8, (uint8_t *) taint_temp);
	  }
	  return;
}

void REGPARM __taint_ldb_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_ldb_raw_paddr(addr,vaddr);

}

void REGPARM __taint_ldw_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_ldw_raw_paddr(addr,vaddr);
}

void REGPARM __taint_ldl_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_ldl_raw_paddr(addr,vaddr);
}

void REGPARM __taint_ldq_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_ldq_raw_paddr(addr,vaddr);
}

void REGPARM __taint_stb_raw_paddr(ram_addr_t addr,gva_t vaddr) {
	if (!taint_memory_page_table || addr >= ram_size)
		return;

	//if (cpu_single_env->tempidx & 0xFF) { fprintf(stderr, "__taint_stb_raw(0x%08x) -> 0x%08x\n", addr, cpu_single_env->tempidx & 0xFF); __asm__ ("int $3"); }
	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
	uint16_t before, after;
	char changed = 0;

	tbitpage_leaf_t *leaf_node = taint_st_general_i32(addr,
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

	//if (cpu_single_env->tempidx & 0xFFFF) {fprintf(stderr, "__taint_stw_raw(0x%08x) -> 0x%08x\n", addr,cpu_single_env->tempidx & 0xFFFF);__asm__ ("int $3");}
	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
  uint16_t before, after;
  char changed = 0;

	tbitpage_leaf_t *leaf_node = taint_st_general_i32(addr,
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

	//if (cpu_single_env->tempidx & 0xFFFFFFFF) {fprintf(stderr, "__taint_stl_raw(0x%08x) -> 0x%08x\n", addr,cpu_single_env->tempidx & 0xFFFFFFFF);__asm__ ("int $3");}
	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
	uint16_t before, after;
	char changed = 0;

	tbitpage_leaf_t *leaf_node = taint_st_general_i32(addr,
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

void REGPARM __taint_stq_raw_paddr(ram_addr_t addr,gva_t vaddr) {
	if (!taint_memory_page_table || addr >= ram_size)
		return;

	/* AWH - Keep track of whether the taint state has changed for this location.
	   If taint was 0 and it is 0 after this store, then change is 0.  Otherwise,
	   it is 1.  This is so any plugins can track that there has been a change
	   in taint. */
	uint16_t before, after;
	char changed = 0;

	tbitpage_leaf_t *leaf_node = NULL, *leaf_node2 = NULL;
	uint32_t taint_temp[2];

	/* AWH - FIXME - BUG - 64-bit stores aren't working right, workaround */
	cpu_single_env->tempidx = 0;
	cpu_single_env->tempidx2 = 0;

#if TCG_TARGET_REG_BITS == 64
	//if (cpu_single_env->tempidx) {fprintf(stderr, "__taint_stq_raw(0x%08x) -> 0x%16x\n", addr, cpu_single_env->tempidx); __asm__ ("int $3");}

	leaf_node = taint_st_general_i32(addr, cpu_single_env->tempidx & 0xFFFFFFFF);
	leaf_node2 = taint_st_general_i32(addr + 4, (cpu_single_env->tempidx & 0xFFFFFFFF00000000) >> 32);
	if (leaf_node)
	*(uint32_t *)(leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)) = (cpu_single_env->tempidx & 0xFFFFFFFF);
	if (leaf_node2)
	*(uint32_t *)(leaf_node2->bitmap + ((addr+4) & LEAF_ADDRESS_MASK)) = (cpu_single_env->tempidx & 0xFFFFFFFF) >> 32;
#else
	//if (cpu_single_env->tempidx || cpu_single_env->tempidx2) {fprintf(stderr, "__taint_stq_raw(0x%08x) -> 0x%08x, 0x%08x\n", addr,cpu_single_env->tempidx, cpu_single_env->tempidx2);__asm__ ("int $3");}

	leaf_node = taint_st_general_i32(addr, cpu_single_env->tempidx);
	leaf_node2 = taint_st_general_i32(addr + 4, cpu_single_env->tempidx2);
	if (leaf_node) {
		before = *(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
		*(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK)) =
				cpu_single_env->tempidx;
		after = *(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
    if ((before != after) || (cpu_single_env->tempidx & 0xFFFFFFFF)) changed = 1;
	}
	if (leaf_node2) {
		before = *(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
		*(uint32_t *) (leaf_node2->bitmap + ((addr+4) & LEAF_ADDRESS_MASK)) =
				cpu_single_env->tempidx2;
		after = *(uint32_t *) (leaf_node->bitmap + (addr & LEAF_ADDRESS_MASK));
		if ((before != after) || (cpu_single_env->tempidx2 & 0xFFFFFFFF)) changed = 1;
	}
#endif /* TCG_TARGET_REG_BITS check */

	if ( changed && DECAF_is_callback_needed (DECAF_WRITE_TAINTMEM_CB) )
	{
		taint_temp[0] = cpu_single_env->tempidx;
		taint_temp[1] = cpu_single_env->tempidx2;
		helper_DECAF_invoke_write_taint_mem(vaddr, addr, 4, taint_temp);
	}
	return;
}

void REGPARM __taint_stb_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_stb_raw_paddr(addr, vaddr);
}

void REGPARM __taint_stw_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_stw_raw_paddr(addr, vaddr);
}

void REGPARM __taint_stl_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_stl_raw_paddr(addr, vaddr);
}

void REGPARM __taint_stq_raw(unsigned long addr, gva_t vaddr) {
	addr = qemu_ram_addr_from_host_nofail((void*)addr);
	__taint_stq_raw_paddr(addr, vaddr);
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
	DECAF_stop_vm();
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
	DECAF_start_vm();
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

int do_taint_pointers(Monitor *mon, const QDict *qdict, QObject **ret_data) {
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

