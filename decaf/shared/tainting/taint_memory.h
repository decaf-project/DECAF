#ifndef __DECAF_TAINT_MEMORY_H__
#define __DECAF_TAINT_MEMORY_H__

#include "qdict.h" // AWH
#include "DECAF_types.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* These were originally in TEMU_main.h */
extern int do_enable_tainting(Monitor *mon, const QDict *qdict, QObject **ret_data);
extern int do_disable_tainting(Monitor *mon, const QDict *qdict, QObject **ret_data);
extern int do_taint_mem_usage(Monitor *mon, const QDict *qdict, QObject **ret_data);
extern int do_taint_nic_on(Monitor *mon, const QDict *qdict, QObject **ret_data);
extern int do_taint_nic_off(Monitor *mon, const QDict *qdict, QObject **ret_data);
extern int do_garbage_collect_taint(Monitor *mon, const QDict *qdict, QObject **ret_data);
extern int do_taint_pointers(Monitor *mon, const QDict *qdict, QObject **ret_data);
extern int do_tainted_bytes(Monitor *mon,const QDict *qdict,QObject **ret_data);
#ifndef qemu_free
extern void qemu_free(void *ptr);
#endif /* qemu_free */
/* The taint shadow memory is represented as a page table with the following
  structure:

  1. The RAM address space can be represented as X number of bits. 1024 megs
  of physical RAM would be represented as 30 bits, for example.

  2. The leaf nodes hold 2 << BITPAGE_LEAF_BITS entries to represent
  2 << BITPAGE_LEAF_BITS bytes of physical RAM address space.

  3. The middle nodes that each hold 2 << BITPAGE_MIDDLE_BITS entries to represent
  2 << BITPAGE_MIDDLE_BITS bytes of physical RAM address space.  Each of those
  entries is a pointer to a leaf node.

  4. The root node, taint_memory_page_table, that has
  2^(X-(2 << (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS))) entries, each of
  which is a middle node.
*/

#define BITPAGE_LEAF_BITS TARGET_PAGE_BITS
#define BITPAGE_MIDDLE_BITS (32-TARGET_PAGE_BITS)/2

/* In order to speed up the page table, we pre-allocate middle and leaf nodes
  in two pools.  The size of these pools (in terms of nodes) is set by the
  following two defines. */
#define BITPAGE_LEAF_POOL_SIZE 100
#define BITPAGE_MIDDLE_POOL_SIZE 50

/* Leaf node for holding memory taint information */
typedef struct _tbitpage_leaf {
  uint8_t bitmap[2 << BITPAGE_LEAF_BITS]; /* This is the bitwise tainting data for the page */
} tbitpage_leaf_t;

/* Middle node for holding memory taint information */
typedef struct _tbitpage_middle {
  tbitpage_leaf_t *leaf[2 << BITPAGE_MIDDLE_BITS];
} tbitpage_middle_t;

/* Pre-allocated pools for leaf and middle nodes */
typedef struct _tbitpage_leaf_pool {
  uint32_t next_available_node;
  tbitpage_leaf_t *pool[BITPAGE_LEAF_POOL_SIZE];
} tbitpage_leaf_pool_t;

typedef struct _tbitpage_middle_pool {
  uint32_t next_available_node;
  tbitpage_middle_t *pool[BITPAGE_MIDDLE_POOL_SIZE];
} tbitpage_middle_pool_t;

extern const uint32_t LEAF_ADDRESS_MASK;
extern const uint32_t MIDDLE_ADDRESS_MASK;
extern tbitpage_middle_t **taint_memory_page_table;
extern tbitpage_leaf_pool_t leaf_pool;
extern tbitpage_middle_pool_t middle_pool;

extern uint32_t leaf_nodes_in_use;
extern uint32_t middle_nodes_in_use;

extern void allocate_leaf_pool(void);
extern void allocate_middle_pool(void);

extern int taint_tracking_enabled;
extern int taint_nic_enabled;
extern int taint_load_pointers_enabled;
extern int taint_store_pointers_enabled;

/* This allocates the root node and its resources */
extern void do_enable_tainting_internal(void);

/* This deallocates all of the nodes in the tree, including the root */
extern void do_disable_tainting_internal(void);

int is_physial_page_tainted(ram_addr_t addr);

/* This deallocates nodes that do not contain taint */
void garbage_collect_taint(int flag);

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

static inline tbitpage_leaf_t *taint_st_general_i32(const uint32_t address, const uint32_t taint) {
  unsigned int middle_node_index = address >> (BITPAGE_LEAF_BITS + BITPAGE_MIDDLE_BITS);
  unsigned int leaf_node_index = (address >> BITPAGE_LEAF_BITS) & MIDDLE_ADDRESS_MASK;
  tbitpage_leaf_t *leaf_node;

  /* Does a middle node exist for this address? */
  if (taint_memory_page_table[middle_node_index]) {
    /* Does a leaf node exist for this address? */
    if (taint_memory_page_table[middle_node_index]->leaf[leaf_node_index]) {
      leaf_node = taint_memory_page_table[middle_node_index]->leaf[leaf_node_index];
    /* Is there no leaf node and no taint to add? */
    } else if (!taint)
      return NULL;
    /* Pull leaf node from pool and put taint in it */
    else {
      leaf_node = fetch_leaf_node_from_pool();
      taint_memory_page_table[middle_node_index]->leaf[leaf_node_index] = leaf_node;
      /* Now we are writing a taint into a newly allocated leaf node. We should flush the TLB entry,
         so the related entry will be marked as io_mem_taint. The virtual address is stored in env->mem_io_vaddr,
         because all tainted memory write either go through io_mem_taint or io_mem_nortdirty.
         FIXME: what about DMA? There will be no related virtual address.
      */
      tlb_flush_page(cpu_single_env, cpu_single_env->mem_io_vaddr);
    }
  /* Is there no middle node and no taint to add? */
  } else if (!taint)
    return NULL;
  /* Pull middle node from pool */
  else {
    leaf_node = fetch_leaf_node_from_pool();
    taint_memory_page_table[middle_node_index] = fetch_middle_node_from_pool();
    taint_memory_page_table[middle_node_index]->leaf[leaf_node_index] = leaf_node;
    /* Now we are writing a taint into a newly allocated leaf node. We should flush the TLB entry,
       so the related entry will be marked as io_mem_taint (or io_mem_notdirty). The virtual address is stored in env->mem_io_vaddr,
       because all tainted memory writes either go through io_mem_write (i.e., io_mem_taint or io_mem_notdirty).
       FIXME: what about DMA? There will be no related virtual address.
    */
    tlb_flush_page(cpu_single_env, cpu_single_env->mem_io_vaddr);
  }
  return leaf_node;
}

/* RAM tainting functions */
#ifdef CONFIG_TCG_TAINT
void REGPARM __taint_ldb_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_ldw_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_ldl_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_ldq_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_ldb_raw_paddr(ram_addr_t addr,gva_t vaddr);
void REGPARM __taint_ldw_raw_paddr(ram_addr_t addr,gva_t vaddr);
void REGPARM __taint_ldl_raw_paddr(ram_addr_t addr,gva_t vaddr);
void REGPARM __taint_ldq_raw_paddr(ram_addr_t addr,gva_t vaddr);

void REGPARM __taint_stb_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_stw_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_stl_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_stq_raw(unsigned long addr, gva_t vaddr);
void REGPARM __taint_stb_raw_paddr(ram_addr_t addr,gva_t vaddr);
void REGPARM __taint_stw_raw_paddr(ram_addr_t addr,gva_t vaddr);
void REGPARM __taint_stl_raw_paddr(ram_addr_t addr,gva_t vaddr);
void REGPARM __taint_stq_raw_paddr(ram_addr_t addr,gva_t vaddr);

void REGPARM taint_mem(ram_addr_t addr, int size, uint8_t *taint);
void REGPARM taint_mem_check(ram_addr_t addr, uint32_t size, uint8_t * taint);

#endif /* CONFIG_TCG_TAINT */
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DECAF_TAINT_MEMORY_H__ */
