/*
 * cfi.h
 *
 *  Created on: Sep 17, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 */

#ifndef CFI_H_
#define CFI_H_

#include <inttypes.h>
#include <glib.h>

#define MAX_THREADS 0xffff

struct Data {
	uint32_t data;
	uint32_t esp;
};

/* Stack Impl */
struct Stack {
    struct Data	*data;
    uint32_t     size;
    uint32_t max_size;
    uint32_t start;
    uint32_t end;
    uint32_t tid;
    uint32_t fiber_id;
    GHashTable *ht;
    struct Stack *next;
    struct Stack *prev;
};
typedef struct Stack Stack;

struct thread {
	uint32_t kernel_stack;
	uint32_t user_stack;
	Stack *ustack;
	Stack *kstack;
	uint32_t tid;
};
typedef struct thread Thread;

#define MAX_REGIONS 30000
struct dyn_region {
	int mem_regions_count;
	uint32_t mem_regions[2 * MAX_REGIONS];
};

#define MAX_MODULES 1000
struct proc_entry {
	char name[128];
	uint32_t modules[2 * MAX_MODULES];
	uint32_t module_count;
	uint32_t pid;
	uint32_t cr3;
	uint32_t curr_tid;
	uint32_t initialized;
	GHashTable *mod_hashtable;
	GHashTable *misc_whitelist;
	Thread *threads[MAX_THREADS];
	struct dyn_region dr;
};

#define NAME_SIZE 1024
struct bin_file {
	char name[NAME_SIZE];
	uint32_t image_base;
	unsigned int reloc_tbl_count;
	unsigned int exp_tbl_count;
	GHashTable *whitelist;
	uint32_t *reloc_tbl;
	uint32_t *exp_tbl;
};

#endif /* CFI_H_ */
