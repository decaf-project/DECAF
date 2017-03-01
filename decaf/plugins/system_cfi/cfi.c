/*
 * commands.c
 *
 *  Created on: May 22, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 *      This file has the implementation for the whitelist plugin
 */

#include "config.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

//For whitelist
#include <dirent.h>
#include <glib.h>

#include <mcheck.h>

#include "procmod.h"
// AWH #include "libfstools.h"

#include "TEMU_main.h"

#include "hookapi.h"
#include "hooks/function_map.h"

#include "libdasm.h"
#include "qapi-types.h"
#include "peheaders.h"

#include "cfi.h"


/* Hash table to hold the cr3 to process entry mapping */
GHashTable *cr3_pe_ht;

/* Hash table to hold the full file name to file entry */
GHashTable *filemap_ht;

/* Hash table to keep track of clashes. Shouldn't exist. :( */
GHashTable *vio_ht;

/* Some counters to keep track of hits/misses */
uint32_t hit_counter = 0;
uint32_t miss_counter = 0;
uint32_t stack_match = 0;
uint32_t miss_ret1 = 0, miss_ret2 = 0;
uint32_t dyn_hit_counter = 0;
uint32_t matched_call_ret = 0;
uint32_t stack_top = 0;
uint32_t stack_linear = 0;
uint32_t st_contains = 0;
extern uint32_t system_cr3;
struct proc_entry *sys_proc_cfi = NULL;
uint32_t system_loaded = 0;

/* Plugin interface */
static plugin_interface_t wl_interface;
int io_logging_initialized = 0;
uint32_t total_count=0;

unsigned long call_count = 0;
unsigned long ret_count = 0;

char current_mod[32] = "";
char current_proc[32] = "";
//char C_DRIVE[256] = "\\home\\aravind\\win7_fs";
char C_DRIVE[256] = "\\home\\aravind\\mnt\\";
char * FOLDER="/home/aravind/Desktop/whitelists/";

static char wl_dir[256];
uintptr_t hndl = 0;

static mon_cmd_t wl_info_cmds[] = {
  {NULL, NULL, },
};

static int wl_init(void);
static void cfi_cleanup(void);
extern void WL_cleanUp();
extern void recon_init();

extern uint32_t* WL_Extract(char *file_name, uint32_t* entries, uint32_t *code_base, struct bin_file *file);
int enum_exp_table_reloc_table_to_wl (char *filename, uint32_t base, char *name, struct bin_file *file);

inline int opt_guest_mem_read(CPUState *env, target_ulong addr,
                        uint8_t *buf, int len, int is_write)
{
	if(TEMU_read_mem(addr, len, buf) < 0) {
		return cpu_memory_rw_debug(env, addr, buf, len, 0);
	}
	return 0;
}

static inline int get_insn_len(uint8_t *insn_bytes)
{
	INSTRUCTION inst;
	int len;
	len = get_instruction(&inst, insn_bytes, MODE_32);
	return len;
}

static uint32_t wl_get_tid()
{
	uint32_t tid = 0;
	uint32_t fs_base = 0, prcb = 0, ethr = 0;
	fs_base = ((SegmentCache *)(TEMU_cpu_segs + R_FS))->base;
	if(!TEMU_is_in_kernel()) {
		cpu_memory_rw_debug(cpu_single_env, fs_base+0x24, (uint8_t *)(&tid), 4, 0);
	} else {
		cpu_memory_rw_debug(cpu_single_env, fs_base+0x20, (uint8_t *)(&prcb), 4, 0);
		cpu_memory_rw_debug(cpu_single_env, prcb+0x4, (uint8_t *)(&ethr), 4, 0);
		cpu_memory_rw_debug(cpu_single_env, ethr+0x22c+0x4, (uint8_t *)(&tid), 4, 0);
	}
	return tid & 0xffff;
}

static uint32_t wl_get_stack_top()
{
	uint32_t fs_base = ((SegmentCache *)(TEMU_cpu_segs + R_FS))->base;
	uint32_t stack_top = 0;
	cpu_memory_rw_debug(cpu_single_env, fs_base+0x04, (uint8_t *)&stack_top, 4, 0);
	return stack_top;
}

static uint32_t wl_get_stack_bottom()
{
	uint32_t fs_base = ((SegmentCache *)(TEMU_cpu_segs + R_FS))->base;
	uint32_t stack_bottom = 0;
	cpu_memory_rw_debug(cpu_single_env, fs_base+0x08, (uint8_t *)&stack_bottom, 4, 0);
	return stack_bottom;
}

static uint32_t wl_get_fiber_data()
{
	uint32_t fs_base = ((SegmentCache *)(TEMU_cpu_segs + R_FS))->base;
	uint32_t fiber_data = 0;
	cpu_memory_rw_debug(cpu_single_env, fs_base+0x10, (uint8_t *)&fiber_data, 4, 0);
	return fiber_data;
}

#define TWICE_MAX_OBJECTS 10000
target_ulong heap_objects[TWICE_MAX_OBJECTS];
int number_of_heap_objects = 0;

int binsearch_mr (target_ulong *A, target_ulong value, int max_elements)
{
	int low = 0;
	int mid = 0;
	int high = max_elements - 1;
	while (low <= high) {
		mid = (low + high)/2;
		if(A[mid] > value)
			high = mid - 1;
		else if(A[mid] < value)
			low = mid + 1;
		else
			return mid;
	}
	return -(low);
}

static void print_generic_table(GHashTable *ht)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, ht);
	while (g_hash_table_iter_next (&iter, &key, &value))
	  {
		monitor_printf(default_mon, "(0x%08x, 0x%08x), ", (uint32_t)key, (uint32_t)value);
	  }
	monitor_printf(default_mon, "\n");
}

static uint32_t lookup_in_whitelist (struct proc_entry *p, uint32_t val)
{
	uint32_t ret = 0;
	int index;
	struct bin_file *file = NULL;
	if(g_hash_table_lookup(p->misc_whitelist, val)) {
		ret = 1;
		goto done;
	}

	index = binsearch_mr(p->modules, val, p->module_count);
	if(index == 0 || index == -1 * (p->module_count)) {
		goto done;
	} else {
		index = (index > 0)? index : -(index);
		if(!(index & 0x1))
			goto done;

		file = g_hash_table_lookup(p->mod_hashtable, p->modules[index - 1]);
		if(!file) {
			//FIXME: This needs to be handled. Possibly a race condition.
			//monitor_printf(default_mon, "Corresponding module file not present... next_eip: 0x%08x\n", val);
			//print_generic_table(p->mod_hashtable);
			//vm_stop();
		} else {
			if(g_hash_table_lookup(file->whitelist, val - p->modules[index - 1])) {
				ret = 1;
				goto done;
			}
		}
	}

done:
	return ret;
}

/*
 * Adds a particular memory region to the monitored regions.
 */
int add_monitored_region(struct proc_entry *p, uint32_t addr, uint32_t size)
{
	int index;

	index = binsearch_mr(p->dr.mem_regions, addr, p->dr.mem_regions_count);

	if(index > 0 || ((-index) & 0x1) == 1)
		return -1;

	if(p->dr.mem_regions_count > (2 * MAX_REGIONS) - 2)
		return -1;

	index = -(index);

	memmove(&((p->dr.mem_regions)[index+2]), &((p->dr.mem_regions)[index]), (p->dr.mem_regions_count - index) * sizeof(target_ulong));
	(p->dr.mem_regions)[index] = addr;
	(p->dr.mem_regions)[index+1] = addr + size;
	p->dr.mem_regions_count += 2;

	return 0;
}

/*
 * Adds a particular module to a process.
 */
int add_proc_module(struct proc_entry *p, uint32_t addr, uint32_t size)
{
	int index;

	index = binsearch_mr(p->modules, addr, p->module_count);

	if(index > 0 || ((-index) & 0x1) == 1)
		return -1;

	if(p->module_count > (2 * MAX_REGIONS) - 2)
		return -1;

	index = -(index);

	memmove(&((p->modules)[index+2]), &((p->modules)[index]), (p->module_count - index) * sizeof(target_ulong));
	(p->modules)[index] = addr;
	(p->modules)[index+1] = addr + size;
	p->module_count += 2;

	return 0;
}

/*
 * Removes a particular memory region from the list of active allocations.
 */
int remove_monitored_region(struct proc_entry *p, uint32_t addr)
{
	int index;
	index = binsearch_mr(p->dr.mem_regions, addr, p->dr.mem_regions_count);
	if((index >= 0) && ((index & (0x1)) == 0)) {
		if((p->dr.mem_regions)[index] == addr) {
			memmove(&((p->dr.mem_regions)[index]), &((p->dr.mem_regions)[index+2]), (p->dr.mem_regions_count - index - 2) * sizeof(target_ulong));
			memset(&((p->dr.mem_regions)[p->dr.mem_regions_count - 2]), 0, 2 * sizeof(target_ulong));
			p->dr.mem_regions_count -= 2;
		}
	}
}

void Stack_Init(Stack *s[0xffff], uint32_t tid, uint32_t fiber_id, uint32_t max_size)
{
	Stack *curr; //, *next;
	if(s[tid] == NULL) {
		s[tid] = (Stack *) malloc (sizeof (Stack));
		s[tid]->max_size = max_size;
		s[tid]->data = (struct Data *) malloc (max_size * sizeof(struct Data));
		memset(s[tid]->data, 0, max_size * sizeof(struct Data));
		s[tid]->size = 0;
		s[tid]->fiber_id = fiber_id;
		s[tid]->next = s[tid]->prev = s[tid];
	} else { //Thread exists, create new fiber and insert at tail. Assume that the fiber doesn't exist.
		curr = (Stack *) malloc (sizeof (Stack));
		curr->max_size = max_size;
		curr->data = (struct Data *) malloc (max_size * sizeof(struct Data));
		memset(curr->data, 0, max_size * sizeof(struct Data));
		curr->size = 0;
		curr->fiber_id = fiber_id;

		curr->next = s[tid];
		curr->prev = s[tid]->prev;
		s[tid]->prev = curr;
	}
}

static Stack *get_fiber_stack(Stack *s, uint32_t fiber_id) {
	Stack *curr, *next;
	if(s == NULL || s->fiber_id == fiber_id)
		return s;
	curr = s;
	while((next = curr->next) != s) {
		if(next->fiber_id == fiber_id)
			return next;
		curr = next;
	}
	//It turns out that fiber_data isn't quite fiber id. :( If a fiber id is not found, then, use the default value.
	return s;
}

uint32_t Stack_Contains(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id, uint32_t next_eip)
{
	unsigned int i;
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL) { //Stack not initialized. Return 0
		goto not_present;
	}
	for(i = 0; i < fiber_stack->size; i++) {
		if(fiber_stack->data[i].data == next_eip)
			return 1;
	}
not_present:
	return 0;
}

uint32_t Stack_Pop(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id)
{
	uint32_t ret;
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL || fiber_stack->data == NULL || fiber_stack->size == 0) {
        fprintf(stderr, "Stack pop error()\n");
        return 0xFFFFFFFF;
    }
    ret = fiber_stack->data[fiber_stack->size-1].data;
    fiber_stack->size--;
    if(fiber_stack->size == 0) {
    	free(fiber_stack->data);
    	fiber_stack->prev->next = fiber_stack->next;
    	fiber_stack->next->prev = fiber_stack->prev;
    	if(fiber_stack == s[tid])
    		s[tid] = NULL;
    	free(fiber_stack);
    }
    return ret;
}

struct Data Stack_Pop_with_esp(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id)
{
	struct Data ret;
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL || fiber_stack->data == NULL || fiber_stack->size == 0) {
	       fprintf(stderr, "Stack pop error()\n");
	       ret.data = 0xFFFFFFFF;
	       return ret;
	}
	memcpy(&ret, &(fiber_stack->data[fiber_stack->size-1]), sizeof(struct Data));
	fiber_stack->size--;
	if(fiber_stack->size == 0) {
		free(fiber_stack->data);
		fiber_stack->prev->next = fiber_stack->next;
		fiber_stack->next->prev = fiber_stack->prev;
		if(fiber_stack == s[tid])
			s[tid] = NULL;
		free(fiber_stack);
	}
	return ret;
}

static void reset_fiber_stack(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id)
{
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL || fiber_stack->data == NULL || fiber_stack->size == 0) {
		   fprintf(stderr, "reset_fiber_stack: Stack pop error()\n");
	}

	free(fiber_stack->data);
	fiber_stack->prev->next = fiber_stack->next;
	fiber_stack->next->prev = fiber_stack->prev;
	if(fiber_stack == s[tid])
		s[tid] = NULL;
	free(fiber_stack);
}


struct Data Stack_MultiPop_till(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id, uint32_t eip)
{
	struct Data ret, popval;
	ret.data = ret.esp = 0;
	while(ret.data != 0xFFFFFFFF) {
		popval = Stack_Pop_with_esp(s, tid, fiber_id);
		if(popval.data == eip)
			return popval;
	}
//	monitor_printf(default_mon,"\n");
	return ret;
}

uint32_t Stack_Top(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id)
{
	struct Data *data;
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL || fiber_stack->data == NULL || fiber_stack->size == 0) {
		fprintf(stderr, "Stack pop error()\n");
		return 0xFFFFFFFF;
	}
	data = fiber_stack->data;
	return data[fiber_stack->size-1].data;
}

struct Data Stack_Top_with_esp(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id)
{
	struct Data ret;
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL || fiber_stack->data == NULL || fiber_stack->size == 0) {
		   fprintf(stderr, "Stack pop error()\n");
		   ret.data = 0xFFFFFFFF;
		   return ret;
	}
	memcpy(&ret, &(fiber_stack->data[fiber_stack->size-1]), sizeof(struct Data));
	return ret;
}

uint32_t Stack_esp(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id)
{
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL || fiber_stack->data == NULL || fiber_stack->size == 0) {
		fprintf(stderr, "Stack pop error()\n");
		return 0xFFFFFFFF;
	}
	return fiber_stack->data[fiber_stack->size - 1].esp;
}

struct Data Stack_Pop_until(Stack *s, int index)
{
	int i = 0;
	struct Data ret;
	ret.data = ret.esp = 0;

	if(index >= s->size) {
		monitor_printf(default_mon, "Invalid index. %d in stack 0x%08x\n", index, s);
		vm_stop(0);
	}

	for(i = s->size-1; i >= index; i--) {
		ret.data = s->data[i].data;
		ret.esp = s->data[i].esp;
		g_hash_table_remove(s->ht, ret.esp);
	}
	s->size = index;

	return ret;
}

int Stack_Push_new(Stack *s, uint32_t esp, uint32_t addr)
{
	int i = 0;
	if(s->data == NULL) {
		s->data = (struct Data *) malloc (sizeof(struct Data) * s->max_size);
	}
	if(s->size == s->max_size) { //Maximum size reached. Reset stack.
		for(i = 0; i < s->max_size; i++) {
			g_hash_table_remove(s->ht, s->data[i].esp);
		}
		memset(s->data, 0, sizeof(struct Data) * s->max_size);
		s->size = 0;
	}
	s->data[s->size].data = addr;
	s->data[s->size].esp = esp;
	s->size++;

	return s->size - 1;
}

void Stack_Push(Stack *s[MAX_THREADS], uint32_t tid, uint32_t fiber_id, uint32_t d, uint32_t esp)
{
	Stack *fiber_stack;
	if(s[tid] == NULL) {
		Stack_Init(s, tid, fiber_id, 1000); //start with 10000 elements
	}
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	uint32_t cur_size = fiber_stack->size;
    if (fiber_stack->size < fiber_stack->max_size) {
    	fiber_stack->data[cur_size].data = d;
    	fiber_stack->data[cur_size].esp = esp;
    	fiber_stack->size++;
    }
    else {
    	fiber_stack->max_size = cur_size = 1000;
    	fiber_stack->size = 0;
        fprintf(stderr, "Stack full. Reseting stack to size:%d\n", fiber_stack->max_size);
        memset(&(fiber_stack->data[cur_size]), 0, fiber_stack->max_size * sizeof(struct Data));
        fiber_stack->data[cur_size].data = d;
        fiber_stack->data[cur_size].esp = esp;
        fiber_stack->size++;
    }
}

/* END - Stack Impl */

int is_kernel_instruction()
{
    return ((*TEMU_cpu_hflags & HF_CPL_MASK) != 3);
}

static inline void dump_stack (
		uint32_t cr3,
		char *name,
		Stack *s[MAX_THREADS],
		uint32_t tid,
		uint32_t fiber_id,
		uint32_t eip,
		uint32_t next_eip,
		uint32_t esp )
{
	unsigned int i;
	Stack *fiber_stack;
	fiber_stack = get_fiber_stack(s[tid], fiber_id);
	if(fiber_stack == NULL) { //Stack not initialized. Return 0
		goto not_present;
	}
	monitor_printf(default_mon, "MISMATCH in %s:%d. CR3: 0x%08x, EIP: 0x%08x, NEXT_EIP: 0x%08x. ESP: 0x%08x ", name, tid, cr3, eip, next_eip, esp);
	for(i = 0; i < fiber_stack->size; i++) {
		monitor_printf(default_mon, "(0x%08x, 0x%08x), ", fiber_stack->data[i].data, fiber_stack->data[i].esp);
	}
	monitor_printf(default_mon, "\n");
not_present:
	return;
}


static Thread *alloc_thread (uint32_t tid)
{
	Thread *thread = NULL;
	thread = (Thread *) malloc (sizeof (Thread));
	memset(thread, 0, sizeof(Thread));
	thread->ustack = (Stack *) malloc (sizeof(Stack));
	thread->kstack = (Stack *) malloc (sizeof(Stack));
	memset(thread->ustack, 0, sizeof(Stack));
	memset(thread->kstack, 0, sizeof(Stack));
	thread->ustack->tid = thread->kstack->tid = tid;
	thread->ustack->max_size = thread->kstack->max_size = 1000;
	thread->ustack->ht = g_hash_table_new(0,0);
	thread->kstack->ht = g_hash_table_new(0,0);
	thread->tid = tid;
	return thread;
}

static inline Stack *get_curr_stack(Thread *th)
{
	if(!TEMU_is_in_kernel())
		return th->ustack;
	else
		return th->kstack;
}

extern void vm_stop(RunState r);
void ret_target_handler(uint32_t eip, uint32_t next_eip, uint32_t op, uint32_t espval)
{
	uint32_t prev_size, new_size, tid;
	tmodinfo_t *tp;
	uint32_t index, in_dyn_mem = 1;
	gboolean ispresent = 0;
	char name[256];
	uint32_t fiber_id;
	uint32_t tp_src = 0;
	struct Data ret_pop;
	if(next_eip == 0)
		goto done;

	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		goto done;
	}

	if(p->initialized != 0x3)
		goto done;

	ret_count++;
	uint32_t esp = 0, esp_page = 0, not_curr_page = 0;
	Stack *st = NULL;
	esp = espval;
	esp_page = (esp >> 3) << 3;
	if(esp_page != p->threads[p->curr_tid]->kernel_stack
			&& esp_page != p->threads[p->curr_tid]->user_stack) { //Still in the same thread. Push to stack and be done.
		p->curr_tid = wl_get_tid();
		if(!p->threads[p->curr_tid]) {
			p->threads[p->curr_tid] = alloc_thread(p->curr_tid);
			uint32_t ret = lookup_in_whitelist(p, next_eip);
			if(!ret) {
				miss_ret1++;
			} else {
				stack_match++;
			}
			goto done;
		}
		if(TEMU_is_in_kernel())
			p->threads[p->curr_tid]->kernel_stack = esp_page;
		else
			p->threads[p->curr_tid]->user_stack = esp_page;
		not_curr_page = 1;
	}
	st = get_curr_stack(p->threads[p->curr_tid]);
	index = g_hash_table_lookup(st->ht, esp);
	if(!index) {
		uint32_t ret = lookup_in_whitelist(p, next_eip);
		if(!ret) {
			miss_ret2++;
		} else {
			stack_match++;
		}
		goto done;
	}

	if(index == 0xabcdef)
		index = 0;

	if(index == st->size - 1)
		stack_top++;

	ret_pop = Stack_Pop_until(st, index);
	if(ret_pop.data != next_eip) {
		uint32_t ret = lookup_in_whitelist(p, next_eip);//g_hash_table_lookup(p->hashtable, (gconstpointer) next_eip);
		if(!ret) {
			index = binsearch_mr(p->dr.mem_regions, next_eip, p->dr.mem_regions_count);
			if(index == 0 || index == -1 * (p->dr.mem_regions_count)) {
				in_dyn_mem = 0;
			}
			else {
				index = (index > 0)? index : -(index);
				if(!(index & 0x1)) {
					in_dyn_mem = 0;
				}
			}

			if(in_dyn_mem) {
				monitor_printf(default_mon, "In dynamic memory. EIP: 0x%08x, next_eip: 0x%08x\n", eip, next_eip);
			} else {
				miss_counter++;
			}
		}
	} else {
		stack_match++;
	}

done:
	return;
}

/* Handler for FLDZ instruction used to store eip */
void floating_point_handler(uint32_t eip, uint32_t next_eip, uint32_t op, uint32_t espval)
{
	uint8_t bytes[15];
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		goto done;
	}

	if(p->initialized != 0x3)
		goto done;

	cpu_memory_rw_debug(env, eip, &bytes[0], 15, 0); //Read the instruction

	if(bytes[0] == 0xd9 && bytes[1] == 0xee)
		g_hash_table_insert(p->misc_whitelist, (gpointer) eip, (gpointer) 2); //2 == FLDZ insn

done:
	return;
}

static inline void update_stack_layout(Stack *st, uint32_t esp)
{
	st->end = (esp >> 3) << 3;
}


void callff_target_handler(uint32_t eip, uint32_t next_eip, uint32_t op, uint32_t espval)
{
	uint32_t fiber_id;
	uint32_t tid;
	uint8_t bytes[15];
	uint32_t esp_page = 0, ret_addr = 0, not_curr_page = 0, esp = 0;
	int index = 0, insn_len = 0, i = 0;
	int in_stack = 1, in_wl = 1, in_dyn_mem = 1;
	tmodinfo_t *tp_src, *tp_dst;
	char name[256];
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	if(next_eip == 0)
		goto done;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		goto done;
	}

	if(p->initialized != 0x3)
		goto done;


	int modrm = (op >> 8) & 0xff;
	Stack *fiber_stack;
	struct Data ret_pop;
	uint32_t prev_size, new_size;

	if(((modrm >> 3) & 7) == 2 || ((modrm >> 3) & 7) == 3 //indirect call insn
			|| ((modrm >> 3) & 7) == 4 || ((modrm >> 3) & 7) == 5) { //indirect jmp insn

		uint32_t ret = lookup_in_whitelist(p, next_eip); //g_hash_table_lookup(p->hashtable, (gconstpointer) next_eip);
		if(!ret) {
			struct proc_entry *system_proc = g_hash_table_lookup(cr3_pe_ht, (gpointer) system_cr3);
			if(system_proc)
				ret = lookup_in_whitelist(system_proc, next_eip); //g_hash_table_lookup(system_proc->hashtable, (gconstpointer) next_eip);

			if(!ret)
				in_wl = 0;
		}

		if(!in_wl) { //Check if in dynamic memory
			index = binsearch_mr(p->dr.mem_regions, next_eip, p->dr.mem_regions_count);
			if(index == 0 || index == -1 * (p->dr.mem_regions_count)) {
				in_dyn_mem = 0;
			}
			else {
				index = (index > 0)? index : -(index);
				if(!(index & 0x1)) {
					in_dyn_mem = 0;
				}
			}
		}

	}

	Stack *st;
	if(((modrm >> 3) & 7) == 2 || ((modrm >> 3) & 7) == 3) { //If it is a call insn, push ret addr to stack
		esp = env->regs[R_ESP];
		esp_page = (esp >> 3) << 3;
		cpu_memory_rw_debug(env, eip, &bytes[0], 15, 0); //Read the instruction
		insn_len = get_insn_len(bytes);
		ret_addr = eip + insn_len;
		if(esp_page != p->threads[p->curr_tid]->kernel_stack
				&& esp_page != p->threads[p->curr_tid]->user_stack) { //Still in the same thread. Push to stack and be done.
			p->curr_tid = wl_get_tid();
			if(!p->threads[p->curr_tid])
				p->threads[p->curr_tid] = alloc_thread(p->curr_tid);
			if(TEMU_is_in_kernel())
				p->threads[p->curr_tid]->kernel_stack = esp_page;
			else
				p->threads[p->curr_tid]->user_stack = esp_page;
			not_curr_page = 1;
		}
		st = get_curr_stack(p->threads[p->curr_tid]);
		index = Stack_Push_new(st, esp, ret_addr);
		g_hash_table_insert(st->ht, esp, ((index == 0)? 0xabcdef : index));

		if(not_curr_page) {
			update_stack_layout(st, esp);
		}

		call_count++;

		//and whitelist the ret addr since some rets are incorporated using indirect jumps Eg: rpcrt4.dll::ObjectStubless()
		g_hash_table_insert(p->misc_whitelist, (gpointer) eip+insn_len, (gpointer) 12);
	}

done:
	return;

}

void call_long_target_handler(uint32_t eip, uint32_t next_eip, uint32_t op, uint32_t espval)
{
	monitor_printf(default_mon, "lcall: 0x9a @ EIP: 0x%08x Stopping VM... \n", eip);
	vm_stop(0);
}

void call_target_handler(uint32_t eip, uint32_t next_eip, uint32_t op, uint32_t espval)
{
	int insn_len = 0;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	uint8_t bytes[15] = {'\0'};
	Stack *st = NULL;
	uint32_t esp_page = 0;
	uint32_t ret_addr = 0;
	uint32_t fiber_id, tid;
	int index = 0;
	uint32_t esp = 0, not_curr_page = 0;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		goto done;
	}

	if(p->initialized != 0x3)
		goto done;

	esp = env->regs[R_ESP];
	esp_page = (esp >> 3) << 3;
	ret_addr = eip + 5;
	if(esp_page != p->threads[p->curr_tid]->kernel_stack
			&& esp_page != p->threads[p->curr_tid]->user_stack) { //Still in the same thread. Push to stack and be done.
		p->curr_tid = wl_get_tid();
		if(!p->threads[p->curr_tid])
			p->threads[p->curr_tid] = alloc_thread(p->curr_tid);
		if(TEMU_is_in_kernel())
			p->threads[p->curr_tid]->kernel_stack = esp_page;
		else
			p->threads[p->curr_tid]->user_stack = esp_page;
		not_curr_page = 1;
	}
	st = get_curr_stack(p->threads[p->curr_tid]);
	index = Stack_Push_new(st, esp, ret_addr);
	g_hash_table_insert(st->ht, esp, ((index == 0)? 0xabcdef : index));

	if(not_curr_page) {
		update_stack_layout(st, esp);
	}

	call_count++;

	//and whitelist the ret addr since some rets are incorporated using indirect jumps Eg: rpcrt4.dll::ObjectStubless()
	//FIXME: For now, inserting 1 as the value. Changing to the struct bin_file * could be costly.
	g_hash_table_insert(p->misc_whitelist, (gpointer) eip+insn_len, (gpointer) 11);

done:
	return;
}

static void print_pskeyval(gpointer key, gpointer val, gpointer ud)
{
	struct proc_entry *p = (struct proc_entry *) val;
	monitor_printf(default_mon, "%d\t %s\t 0x%08x\n", p->pid, p->name, p->cr3);
}

void do_pslist_from_hashmap(Monitor *mon, const QDict *qdict)
{
	g_hash_table_foreach(cr3_pe_ht, print_pskeyval, NULL);
}

static void print_wl_entries(gpointer key, gpointer val, gpointer ud)
{
	monitor_printf(default_mon, "0x%08x, ", key);
}

void do_dump_mod_wl(Monitor *mon, const QDict *qdict)
{
#if 0
	GHashTableIter iter;
	gpointer key, value;
	char *name;
	uint32_t base;

	if(qdict_haskey(qdict, "name")) {
		name = qdict_get_str(qdict, "name");
	} else {
		return;
	}
	if(qdict_haskey(qdict, "base")) {
		base = qdict_get_int(qdict, "base");
	} else {
		return;
	}

	g_hash_table_iter_init (&iter, filemap_ht);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{

	}

	g_hash_table_insert(filemap_ht, f->name, f);
#endif
}

static void print_mods(gpointer key, gpointer val, gpointer ud)
{
	struct bin_file *file = (struct bin_file *)val;
	monitor_printf(default_mon, "%s, start: 0x%08x\n", file->name, file->image_base);
}

void do_dump_proc_modules(Monitor *mon, const QDict *qdict)
{
	uint32_t cr3 = 0;
	if(qdict_haskey(qdict, "cr3")) {
		cr3 = qdict_get_int(qdict, "cr3");
	}
	if(cr3 == 0)
		return;
	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, cr3);
	if(p == NULL) {
		monitor_printf(default_mon, "proc not found.\n");
		return;
	}
	if(p->mod_hashtable == NULL) {
		monitor_printf(default_mon, "mod_hashtable is null for %s\n", p->name);
		return;
	}
	g_hash_table_foreach(p->mod_hashtable, print_mods, NULL);
}


static char mon_proc[256];
void do_monitor_proc(Monitor *mon, const QDict *qdict)
{
	const char *name;
	if(qdict_haskey(qdict, "proc_name")) {
		name = qdict_get_str(qdict, "proc_name");
		strncpy(mon_proc, name, strlen(name));
	}
	return;
}

/* TODO: Implement kernel mode on/off */
void do_set_kernel_cfi(Monitor *mon, const QDict *qdict)
{

}

void do_print_tid(Monitor *mon, const QDict *qdict)
{
	monitor_printf(default_mon,"Current thread ID is tid: %x\n", wl_get_tid());
}

void do_set_guest_dir(Monitor *mon, const QDict *qdict)
{
	const char *dir_name = qdict_get_str(qdict, "guest_directory");
	strncpy(C_DRIVE, dir_name, strlen(dir_name));
}

void do_set_wl_dir(Monitor *mon, const QDict *qdict)
{
	const char *dir_name = qdict_get_str(qdict, "whitelist_directory");
	strncpy(wl_dir, dir_name, strlen(dir_name));
}

static void print_keyval(gpointer key, gpointer val, gpointer ud)
{
	monitor_printf(default_mon, "%s\n", (char *) key);
}

static void print_intint(gpointer key, gpointer val, gpointer ud)
{
	monitor_printf(default_mon, "0x%08x, %d\n", key, val);
}

void do_dump_system_dyn_regions(Monitor *mon, const QDict *qdict)
{
	int i;
	if(!system_cr3)
		goto done;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) system_cr3);

	if(!p)
		goto done;

	for(i = 0; i < p->dr.mem_regions_count; i+=2) {
		monitor_printf(default_mon, "0x%08x 0x%08x\n", p->dr.mem_regions[i], p->dr.mem_regions[i+1]);
	}
	monitor_printf(default_mon, "Total number of entries = %d\n", (p->dr.mem_regions_count)/2);

	monitor_printf(default_mon, "Violation ht....\n");
	g_hash_table_foreach(vio_ht, print_intint, NULL);
	monitor_printf(default_mon, "Number of entries in violations ht: %d\n", g_hash_table_size(vio_ht));

done:
	return;
}

#if 0
static unsigned int bin_file_write(FILE *fp, struct bin_file *file)
{
	uint32_t *temp = NULL;
	int i = 0;
	unsigned int size = 0;
	unsigned int byte_count = 0;
	//fwrite((void *)file->name, NAME_SIZE, 1, fp);
	fprintf(fp, "%s\n", file->name);
	byte_count += NAME_SIZE;

//	fwrite((void *)&(file->image_base), sizeof(uint32_t), 1, fp);
	fprintf(fp, "0x%08x\n", file->image_base);
	byte_count += sizeof(uint32_t);

//	fwrite((void *)&(file->reloc_tbl_count), sizeof(unsigned int), 1, fp);
	fprintf(fp, "%d\n", file->reloc_tbl_count);
	byte_count += sizeof(unsigned int);

//	fwrite((void *)file->reloc_tbl, sizeof(uint32_t) * file->reloc_tbl_count, 1, fp);
	//fwrite((void *)&(file->exp_tbl_count), sizeof(unsigned int), 1, fp);
	fprintf(fp, "%d\n", file->exp_tbl_count);
	byte_count += sizeof(unsigned int);

	size = g_hash_table_size(file->whitelist);
	//fwrite((void *)&(size), sizeof(unsigned int), 1, fp);
	fprintf(fp, "%d\n", size);
	byte_count += sizeof(unsigned int);

//	fwrite((void *)file->exp_tbl, sizeof(uint32_t) * file->exp_tbl_count, 1, fp);
	temp = (uint32_t *) malloc (sizeof(uint32_t) * size);

	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, file->whitelist);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		//temp[i++] = (uint32_t)key;
		fprintf(fp, "0x%08x\n", (uint32_t)key);
	}

//	fwrite((void *)temp, i * sizeof(uint32_t), 1, fp);
	byte_count += i * sizeof(uint32_t);

	free(temp);
	monitor_printf(default_mon, "Written %s. Exp tbl: %d, reloc tbl: %d, Entries in ht: %d.\n",
					file->name, file->exp_tbl_count, file->reloc_tbl_count, g_hash_table_size(file->whitelist));

	return byte_count;
}

void load_file_wl (char *file)
{
	FILE *fp = fopen(file, "rb");
	uint32_t ht_size;
	int i;
	uint32_t *temp = NULL;
	unsigned int byte_count = 0;
	unsigned int size = 0;
	if(fread((void *)&ht_size, sizeof(ht_size), 1, fp) <= 0) {
		fclose(fp);
		return;
	}
	monitor_printf(default_mon, "Total files = %d\n", ht_size);

	struct bin_file *f = NULL;


	for(i = 0; i < ht_size; i++) { //Read each file
		f = (struct bin_file *) malloc (sizeof(*f));
		memset(f, 0, sizeof(*f));
		fread((void *)f->name, NAME_SIZE, 1, fp);
		fread((void *)&(f->image_base), sizeof(uint32_t), 1, fp);
		fread((void *)&(f->reloc_tbl_count), sizeof(unsigned int), 1, fp);
		fread((void *)&(f->exp_tbl_count), sizeof(unsigned int), 1, fp);
		fread((void *)&(size), sizeof(unsigned int), 1, fp);
		byte_count += (NAME_SIZE + sizeof(uint32_t) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int));

		temp = (uint32_t *) malloc (sizeof(uint32_t) * size);
		fread((void *)temp, sizeof(uint32_t) * size, 1, fp);
		byte_count += sizeof(uint32_t) * size;

		f->whitelist = g_hash_table_new(0, 0);
		for(i = 0; i < size; i++)
			g_hash_table_insert(f->whitelist, temp[i], 1);
		free(temp);
		monitor_printf(default_mon, "Loaded %s. Exp tbl: %d, reloc tbl: %d, Entries in ht: %d. Total bytes read = %d\n",
				f->name, f->exp_tbl_count, f->reloc_tbl_count, g_hash_table_size(f->whitelist), byte_count);
		g_hash_table_insert(filemap_ht, f->name, f);
	}
	fclose(fp);
	monitor_printf(default_mon, "Total entries loaded = %d\n", g_hash_table_size(filemap_ht));
}

void do_dump_file_wl (Monitor *mon, const QDict *qdict)
{
	GHashTableIter iter;
	gpointer key, value;
	struct bin_file *file;
	FILE *fp = fopen("file_whitelist.dump", "wb");
	uint32_t ht_size = 0;
	unsigned int byte_count = 0;

	ht_size = g_hash_table_size(filemap_ht);
	monitor_printf(default_mon, "Total files in ht = %d\n", ht_size);

	//fwrite((void *)&ht_size, sizeof(ht_size), 1, fp);
	fprintf(fp, "%d\n", ht_size);
	byte_count += sizeof(ht_size);

	g_hash_table_iter_init (&iter, filemap_ht);
	while (g_hash_table_iter_next (&iter, &key, &value))
	  {
		file = (struct bin_file *)value;
		//monitor_printf(default_mon, "(%s, exp=%d, reloc=%d), ", file->name, file->exp_tbl_count, file->reloc_tbl_count);
		byte_count += bin_file_write(fp, file);
	  }

	monitor_printf(default_mon, "Total bytes written = %d\n", byte_count);

	fclose(fp);
}
#endif

static unsigned int bin_file_write (FILE *fp, struct bin_file *file)
{
	uint32_t *temp = NULL;
	int i = 0;
	unsigned int size = 0;
	unsigned int byte_count = 0;
	fwrite((void *)file->name, NAME_SIZE, 1, fp);
	byte_count += NAME_SIZE;

	fwrite((void *)&(file->image_base), sizeof(uint32_t), 1, fp);
	byte_count += sizeof(uint32_t);

	fwrite((void *)&(file->reloc_tbl_count), sizeof(unsigned int), 1, fp);
	byte_count += sizeof(unsigned int);

	fwrite((void *)&(file->exp_tbl_count), sizeof(unsigned int), 1, fp);
	byte_count += sizeof(unsigned int);

	size = g_hash_table_size(file->whitelist);
	fwrite((void *)&(size), sizeof(unsigned int), 1, fp);
	byte_count += sizeof(unsigned int);

	temp = (uint32_t *) malloc (sizeof(uint32_t) * size);

	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, file->whitelist);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		temp[i++] = (uint32_t)key;
	}

	assert(i == size);
	fwrite((void *)temp, i * sizeof(uint32_t), 1, fp);
	byte_count += i * sizeof(uint32_t);

	free(temp);
	monitor_printf(default_mon, "Written %s. Exp tbl: %d, reloc tbl: %d, Entries in ht: %d. Total bytes written = %d\n",
					file->name, file->exp_tbl_count, file->reloc_tbl_count, g_hash_table_size(file->whitelist), byte_count);

	return byte_count;
}

void load_file_wl(char *file)
{
	FILE *fp = fopen(file, "rb");
	uint32_t ht_size;
	int i, j;
	uint32_t *temp = NULL;
	unsigned int byte_count = 0;
	unsigned int size = 0;

	if(!fp)
		return;

	if(fread((void *)&ht_size, sizeof(ht_size), 1, fp) <= 0) {
		fclose(fp);
		return;
	}
	monitor_printf(default_mon, "Total files = %d\n", ht_size);

	struct bin_file *f = NULL;
	for(i = 0; i < ht_size; i++) { //Read each file
		f = (struct bin_file *) malloc (sizeof(*f));
		memset(f, 0, sizeof(*f));
		fread((void *)f->name, NAME_SIZE, 1, fp);
		fread((void *)&(f->image_base), sizeof(uint32_t), 1, fp);
		fread((void *)&(f->reloc_tbl_count), sizeof(unsigned int), 1, fp);
		fread((void *)&(f->exp_tbl_count), sizeof(unsigned int), 1, fp);
		fread((void *)&(size), sizeof(unsigned int), 1, fp);
		byte_count += (NAME_SIZE + sizeof(uint32_t) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int));

		temp = (uint32_t *) malloc (sizeof(uint32_t) * size);
		fread((void *)temp, sizeof(uint32_t) * size, 1, fp);
		byte_count += sizeof(uint32_t) * size;

		f->whitelist = g_hash_table_new(0, 0);
		for(j = 0; j < size; j++)
			g_hash_table_insert(f->whitelist, temp[j], 1);
		free(temp);
		monitor_printf(default_mon, "Loaded %s. Exp tbl: %d, reloc tbl: %d, Entries in ht: %d. Total bytes read = %d\n",
				f->name, f->exp_tbl_count, f->reloc_tbl_count, g_hash_table_size(f->whitelist), byte_count);
		g_hash_table_insert(filemap_ht, f->name, f);
	}
	fclose(fp);
	monitor_printf(default_mon, "Total entries loaded = %d\n", g_hash_table_size(filemap_ht));
}

void do_dump_file_wl(Monitor *mon, const QDict *qdict)
{
	GHashTableIter iter;
	gpointer key, value;
	struct bin_file *file;
	FILE *fp = fopen("file_whitelist.dump", "wb");
	uint32_t ht_size = 0;
	unsigned int byte_count = 0;

	ht_size = g_hash_table_size(filemap_ht);
	monitor_printf(default_mon, "Total files in ht = %d\n", ht_size);

	fwrite((void *)&ht_size, sizeof(ht_size), 1, fp);
	byte_count += sizeof(ht_size);

	g_hash_table_iter_init (&iter, filemap_ht);
	while (g_hash_table_iter_next (&iter, &key, &value))
	  {
		file = (struct bin_file *)value;
		byte_count += bin_file_write(fp, file);
	  }

	monitor_printf(default_mon, "Total bytes written = %d\n", byte_count);

	fflush(fp);
	fclose(fp);
}

void startup_registrations()
{
//	return;

	monitor_printf(default_mon, "Registering for callback handlers...\n");

	register_insn_cb_range(0xe8, 0xe8, call_target_handler);
	register_insn_cb_range(0xff, 0xff, callff_target_handler);
	register_insn_cb_range(0xc2, 0xc3, ret_target_handler);
	register_insn_cb_range(0x9a, 0x9a, call_long_target_handler);

	//Callback to handle floating point eip retrieval
	register_insn_cb_range(0xd9, 0xd9, floating_point_handler);
}

static int wl_init(void)
{

  procmod_init();
  function_map_init();
  init_hookapi();

  recon_init();
  cr3_pe_ht = g_hash_table_new(0, 0);
  filemap_ht = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
  vio_ht = g_hash_table_new(0, 0);

  load_file_wl("file_whitelist.dump");

  return 0;
}

static void cfi_cleanup(void)
{
	int i;

    cleanup_insn_cbs();

   if(hndl)
   	DECAF_unregister_callback(DECAF_BLOCK_BEGIN_CB, hndl);

   recon_cleanup();
}

static mon_cmd_t wl_term_cmds[] = {
#include "wl_cmds.h"
  {NULL, NULL, },
};

struct hook_data {
	uintptr_t handle;
};

static void GetProcAddress_ret_hook(void *opaque)
{
	struct hook_data *hook_handle = (struct hook_data *)opaque;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		goto done;
	}

	if(env->eip != 0)
		g_hash_table_insert(p->misc_whitelist, (gpointer) env->eip, (gpointer) 13);

done:
	hookapi_remove_hook(hook_handle->handle);
	free(hook_handle);
}

static void GetProcAddress_hook(void *opaque)
{
	struct hook_data *hook_handle;
	uint32_t ret_addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	cpu_memory_rw_debug(env, env->regs[R_ESP], (uint8_t *)&ret_addr, 4, 0);
	hook_handle = (struct hook_data *) malloc (sizeof(*hook_handle));
	hook_handle->handle = hookapi_hook_return(ret_addr, GetProcAddress_ret_hook, (void *)hook_handle, sizeof(*hook_handle));
}

struct vp_hook_info {
	uint32_t addr;
	uint32_t size;
	uintptr_t handle;
};

static void VirtualFree_hook(void *opaque)
{
	uint32_t addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	cpu_memory_rw_debug(env, env->regs[R_ESP]+4, (uint8_t *)&addr, 4, 0);

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "VirtualFree_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		goto done;
	}

	remove_monitored_region(p, addr);
done:
	return;
}

static void RtlFreeHeap_hook(void *opaque)
{
	uint32_t addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	cpu_memory_rw_debug(env, env->regs[R_ESP]+12, (uint8_t *)&addr, 4, 0);
	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "RtlFreeHeap_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	remove_monitored_region(p, addr);

done:
	return;
}

static void RtlAllocateHeap_ret_hook(void *opaque)
{
	struct vp_hook_info *hook = (struct vp_hook_info *)opaque;
	uint32_t addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "RtlAllocateHeap_ret_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	if(env->eip != 0)
		add_monitored_region(p, env->eip, hook->size);

done:
	hookapi_remove_hook(hook->handle);
	free(hook);
}

static void VirtualAlloc_ret_hook(void *opaque)
{
	struct vp_hook_info *hook = (struct vp_hook_info *)opaque;
	uint32_t addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "VirtualAlloc_ret_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	if(env->eip != 0) {
		add_monitored_region(p, env->eip, hook->size);
		monitor_printf(default_mon, "Found dyn code region: Base = 0x%08x, size = %d\n", env->eip, hook->size);
	}

done:
	hookapi_remove_hook(hook->handle);
	free(hook);
}


static void VirtualAlloc_hook(void *opaque)
{
	uint32_t prot, ret_addr, esp;
	struct vp_hook_info *hook_handle;

	uint32_t alloc_type, size, lpaddr;

	//Ret hook is same for both VirtualAlloc and RtlAllocateHeap
	//void (*VirtualAlloc_ret_hook)(void *opaque) = RtlAllocateHeap_ret_hook;

	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	esp = env->regs[R_ESP];

	cpu_memory_rw_debug(env, esp, (uint8_t *)&ret_addr, 4, 0);
	cpu_memory_rw_debug(env, esp+16, (uint8_t *)&prot, 4, 0);
	cpu_memory_rw_debug(env, esp+12, (uint8_t *)&alloc_type, 4, 0);
	cpu_memory_rw_debug(env, esp+8, (uint8_t *)&size, 4, 0);
	cpu_memory_rw_debug(env, esp+4, (uint8_t *)&lpaddr, 4, 0);

	monitor_printf(default_mon, "VirtualAlloc(0x%08x, %d, %x, %x)\n", lpaddr, size, alloc_type, prot);

	if(prot & 0x000000F0) { //Has execute permission. Hook return
		hook_handle = (struct vp_hook_info *) malloc (sizeof(*hook_handle));
		memset(hook_handle, 0, sizeof(*hook_handle));
		cpu_memory_rw_debug(env, esp+8, (uint8_t *)&(hook_handle->size), 4, 0);
		hook_handle->handle = hookapi_hook_return(ret_addr, VirtualAlloc_ret_hook, (void *)hook_handle, sizeof(*hook_handle));
	}
}

static void RtlAllocateHeap_hook(void *opaque)
{
	struct vp_hook_info *hook_handle;
	uint32_t ret_addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	cpu_memory_rw_debug(env, env->regs[R_ESP], (uint8_t *)&ret_addr, 4, 0);
	hook_handle = (struct vp_hook_info *) malloc (sizeof(*hook_handle));
	cpu_memory_rw_debug(env, env->regs[R_ESP]+12, (uint8_t *)&(hook_handle->size), 4, 0);

	hook_handle->handle = hookapi_hook_return(ret_addr, RtlAllocateHeap_ret_hook, (void *)hook_handle, sizeof(*hook_handle));
}

static void MapViewOfFile_ret_hook(void *opaque)
{
	struct vp_hook_info *hook = (struct vp_hook_info *)opaque;
	uint32_t addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "MapViewOfFile_ret_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	if(env->eip != 0) {
		if(hook->size == 0)
			add_monitored_region(p, env->eip, 100*4096);
	}

done:
	hookapi_remove_hook(hook->handle);
	free(hook);
}

static void MapViewOfFile_hook(void *opaque)
{
	struct vp_hook_info *hook_handle;
	uint32_t ret_addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	cpu_memory_rw_debug(env, env->regs[R_ESP], (uint8_t *)&ret_addr, 4, 0);
	hook_handle = (struct vp_hook_info *) malloc (sizeof(*hook_handle));
	cpu_memory_rw_debug(env, env->regs[R_ESP]+4, (uint8_t *)&(hook_handle->size), 4, 0);

	hook_handle->handle = hookapi_hook_return(ret_addr, MapViewOfFile_ret_hook, (void *)hook_handle, sizeof(*hook_handle));
}


/*
 * If changing the protection was successful, add it to monitored regions.
 */
static void VirtualProtect_ret_hook(void *opaque)
{
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	struct vp_hook_info *hook_handle = (struct vp_hook_info *) opaque;
	int index;

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "VirtualProtect_ret_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	if(env->eip != 0)
		add_monitored_region(p, hook_handle->addr, hook_handle->size);

done:
	hookapi_remove_hook(hook_handle->handle);
	free(hook_handle);
}

/*
 * When a fiber yields and requests switch to another fiber, the stack is manipulated and "jumped"
 * to a previously saved address. This address needs to be added into the whitelist since it is
 * a valid indirect jump.
 */
static void SwitchToFiber_hook(void *opaque)
{
//if Win xp
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	uint32_t fiber_data;
	uint32_t branch_addr;
	cpu_memory_rw_debug(env, env->regs[R_ESP] + 4, (uint8_t *)&fiber_data, sizeof(fiber_data), 0);
	cpu_memory_rw_debug(env, fiber_data + 0xcc, (uint8_t *)&branch_addr, sizeof(branch_addr), 0);
	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "SwitchToFiber_hook(eip = 0x%08x, next_eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	g_hash_table_insert(p->misc_whitelist, (gpointer) branch_addr, (gpointer) 14);
done:
	return;
}

static void VirtualProtect_hook(void *opaque)
{
	struct vp_hook_info *hook_handle;
	uint32_t ret_addr;
	uint32_t prot;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;

	cpu_memory_rw_debug(env, env->regs[R_ESP], (uint8_t *)&ret_addr, 4, 0);
	cpu_memory_rw_debug(env, (env->regs[R_ESP])+12, (uint8_t *)&prot, 4, 0);

	if(!(prot & (0x10 | 0x20 | 0x40 | 0x80))) { //Execute bit not set
		return;
	}

	hook_handle = (struct vp_hook_info *) malloc (sizeof(*hook_handle));

	cpu_memory_rw_debug(env, (env->regs[R_ESP])+8, (uint8_t *)&hook_handle->size, 4, 0);
	cpu_memory_rw_debug(env, (env->regs[R_ESP])+4, (uint8_t *)&hook_handle->addr, 4, 0);

	hook_handle->handle = hookapi_hook_return(ret_addr, VirtualProtect_ret_hook, (void *)hook_handle, sizeof(*hook_handle));
}

static void setjmp_hook(void *opaque)
{
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	uint32_t arg, retaddr;
	cpu_memory_rw_debug(env, env->regs[R_ESP]+4, (uint8_t *)&arg, 4, 0);
	cpu_memory_rw_debug(env, arg+0x1c, (uint8_t *)&retaddr, 4, 0);

	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "setjmp_hook(eip = 0x%08x, next_eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	g_hash_table_insert(p->misc_whitelist, (gpointer) retaddr, (gpointer) 1);

done:
	return;
}

static void ExAllocatePoolWithTag_ret_hook(void *opaque)
{
	struct vp_hook_info *hook = (struct vp_hook_info *)opaque;
	uint32_t addr;
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
	if(!p) {
		monitor_printf(default_mon, "ExAllocatePoolWithTag_ret_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
				env->eip, env->cr[3]);
		vm_stop(0);
		goto done;
	}

	if(env->eip != 0) {
		add_monitored_region(p, env->eip, hook->size);
	}

done:
	hookapi_remove_hook(hook->handle);
	free(hook);
}

void ExAllocatePoolWithTag_begin(void *opaque)
{
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	uint32_t ret_addr, esp, size;
	struct vp_hook_info *hook_handle;

	esp = env->regs[R_ESP];
	cpu_memory_rw_debug(env, esp, (uint8_t *)&ret_addr, 4, 0);

	hook_handle = (struct vp_hook_info *) malloc (sizeof(*hook_handle));
	memset(hook_handle, 0, sizeof(*hook_handle));
	cpu_memory_rw_debug(env, esp+8, (uint8_t *)&(hook_handle->size), 4, 0);
	hook_handle->handle = hookapi_hook_return(ret_addr, ExAllocatePoolWithTag_ret_hook, (void *)hook_handle, sizeof(*hook_handle));
}

void ExFreePoolWithTag_begin(void *opaque)
{
	CPUState *env = cpu_single_env ? cpu_single_env : first_cpu;
	uint32_t addr;
	cpu_memory_rw_debug(env, env->regs[R_ESP]+4, (uint8_t *)&addr, 4, 0);
	if(addr) {
		struct proc_entry *p = g_hash_table_lookup(cr3_pe_ht, (gpointer) env->cr[3]);
		if(!p) {
			monitor_printf(default_mon, "RtlFreeHeap_hook(eip = 0x%08x, cr3 = 0x%08x):Process not present... Stopping VM...\n",
					env->eip, env->cr[3]);
			vm_stop(0);
			goto done;
		}
		remove_monitored_region(p, addr);
	}
done:
	return;
}

static void insert_file_to_proc (struct proc_entry *p, struct bin_file *file, uint32_t load_addr)
{
	int i;
	char wFileName[2048] = {'\0'};
	char temp_str[16] = {'\0'};
	uint32_t new_addr = 0;

	if(file->exp_tbl_count == 0 && file->reloc_tbl_count == 0)
		return;


	g_hash_table_insert(p->mod_hashtable, file->image_base, (gpointer)file);

}

static void insert_proc(uint32_t pid, uint32_t cr3, char *name)
{
	struct proc_entry *e = g_hash_table_lookup(cr3_pe_ht, (gpointer) cr3);
	if(e) {
		monitor_printf(default_mon, "insert_proc(pid = %d, name = %s, cr3 = 0x%08x):Process already present... %d, %s, %08x..\n",
				pid, name, cr3, e->pid, e->name, e->cr3);

		if(e->pid != pid) {
			monitor_printf(default_mon, "PID mismatch(e->pid = %d. pid = %d). Shouldn't happen.\n", e->pid, pid);
			vm_stop(0);
		}
		if(e->name[0] == '\0')
			strcpy(e->name, name);

		goto done;
	}

	e = (struct proc_entry *) malloc (sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->cr3 = cr3;
	strcpy(e->name, name);
	e->pid = pid;
	e->misc_whitelist = g_hash_table_new(0, 0);
	e->mod_hashtable = g_hash_table_new(0,0);
	e->threads[e->curr_tid] = alloc_thread(e->curr_tid);

	g_hash_table_insert(cr3_pe_ht, (gpointer) cr3, (gpointer) e);
	if(name[0] != '\0')
		e->initialized |= 0x1;

	if(strcmp(name, "System") == 0)
		sys_proc_cfi = e;

done:
	return;
}

uint32_t insn_cbs_registered = 0;
static void wl_loadmainmodule_notify(uint32_t pid, uint32_t cr3, char *name)
{

	monitor_printf(default_mon, "%s started. PID = %d, cr3 = 0x%08x\n", name, pid, cr3);

	insert_proc(pid, cr3, name);

	/* Hook memory allocation/deallocation functions to monitor dynamic code */
	if(pid == 4) { //system process

	} else {
		hookapi_hook_function_byname(
			"kernel32.dll",
			"GetProcAddress",
			1,
			cr3,
			GetProcAddress_hook,
			NULL,
			0);

		hookapi_hook_function_byname(
			"ntdll.dll",
			"RtlAllocateHeap",
			1,
			cr3,
			RtlAllocateHeap_hook,
			NULL,
			0);

		hookapi_hook_function_byname(
			"ntdll.dll",
			"RtlFreeHeap",
			1,
			cr3,
			RtlFreeHeap_hook,
			NULL,
			0);

		hookapi_hook_function_byname(
			"kernel32.dll",
			"VirtualAlloc",
			1,
			cr3,
			VirtualAlloc_hook,
			NULL,
			0);

		hookapi_hook_function_byname(
			"kernel32.dll",
			"VirtualFree",
			1,
			cr3,
			VirtualFree_hook,
			NULL,
			0);

		hookapi_hook_function_byname(
			"kernel32.dll",
			"VirtualProtect",
			1,
			cr3,
			VirtualProtect_hook,
			NULL,
			0);
	}
}


int enum_exp_table_reloc_table_to_wl (char *filename, uint32_t base, char *name, struct bin_file *file)
{
	char wFileName[1024] = {'\0'};
	strcpy(wFileName, FOLDER);
	strcat(wFileName,name);
	strcat(wFileName,".wl");
	FILE *dll_file;
    dll_file = fopen(wFileName, "w");

	uint32_t et_num, image_base;
	int ret = WL_Extract(filename, &et_num, &image_base, file);
	monitor_printf(default_mon, " Entries from export table = %d and entries from reloc table = %d\n", file->exp_tbl_count, file->reloc_tbl_count);

	return ret;
}


void convert_to_host_filename(char *fullname, char *host_name)
{
	char fullname_lower[512] = {'\0'};
	char temp[1024] = {'\0'};
	int i;

	if(strstr(fullname, "\\") == 0) {
		strcpy(temp, "\\WINDOWS\\system32\\DRIVERS\\");
		strcat(temp, fullname);
		fullname = temp;
	}

	for(i = 0; i < strlen(fullname); i++)
		fullname_lower[i] = tolower(fullname[i]);

	strcpy(host_name, C_DRIVE);
	if(strstr(fullname,"\\Device\\HarddiskVolume2\\")!=0)
	{
	char *first=strstr(fullname,"\\");
    char *second=strstr((first+1),"\\");
    char *third=strstr((second+1),"\\");
    strcat(host_name,third);
	} else
	if(strstr(fullname,"\\Device\\HarddiskVolume1\\")!=0)
	{
	char *first=strstr(fullname,"\\");
    char *second=strstr((first+1),"\\");
    char *third=strstr((second+1),"\\");
    strcat(host_name,third);
	}
	else if(strstr(fullname,"\\SystemRoot\\")!=0)
	{
		char *first=strstr(fullname,"\\");
		    char *second=strstr((first+1),"\\");
		    char *third=strstr((second+1),"\\");
		    strcat(host_name,"\\WINDOWS\\system32");
		    strcat(host_name,third);

	}
	else if(strstr(fullname_lower, "\\") != 0) {
		char *start = strstr(fullname_lower, "\\");
		strcat(host_name, start);
	}
#if 0
	else if(strstr(fullname_upper, "C:\\WINDOWS\\SYSTEM32") != 0)
	{
		char *start = strstr(fullname_upper, "C:\\WINDOWS\\SYSTEM32");
		char *t = start + strlen("C:\\WINDOWS\\SYSTEM32");
		int offset = (int)(t - &fullname[0]);
		strcat(host_name, "\\WINDOWS\\system32");
		strcat(host_name, (char *)(fullname+offset));
		char *curr = host_name;
		while((t = strstr(curr, "\\")) != NULL)
			curr = t + 1;
		while(*curr != '\0') {
			*curr = tolower(*curr);
			curr++;
		}
	}
#endif
	else /*if(strstr(fullname,"\\WINDOWS\\")!=0)*/
	{
		strcat(host_name,fullname);

	}
    int x=0;
    while (host_name[x]!=0) {

    	if (((int)host_name[x])==92) {
                   host_name[x]='/';
                 }

                x++;
               }

    for(i = 0; i < strlen(host_name); i++)
    	host_name[i] = tolower(host_name[i]);

}

void wl_load_module_notify (
		uint32_t pid,
		uint32_t cr3,
		char *name,
		uint32_t base,
		uint32_t size,
		char *fullname)
{

	char host_filename[1024] = {'\0'};
	char temp[1024] = {'\0'};
	struct bin_file *file = NULL;
	char name_lower[256];
	int ret, i;
	struct proc_entry *p = (struct proc_entry *) g_hash_table_lookup(cr3_pe_ht, (gconstpointer) cr3);
	if(p == NULL) {
		monitor_printf(default_mon, "Module loaded: name = %s, pid = %d, cr3 = 0x%08x, base = 0x%08x, fullname = %s. PROCESS NOT FOUND adding...\n",
				name, pid, cr3, base, fullname);
		if(strstr(name, ".exe") || strstr(name, ".EXE"))
			insert_proc(pid, cr3, name);
		else
			insert_proc(pid, cr3, "");
		p = (struct proc_entry *) g_hash_table_lookup(cr3_pe_ht, (gconstpointer) cr3);
	}

	for(i = 0; i < strlen(fullname); i++) {
		if(!isascii(fullname[i]))
			continue;
		temp[i] = tolower(fullname[i]);
	}

	if(strstr(temp, "ntdll.dll"))
		p->initialized |= 0x2;

	file = (struct bin_file *) g_hash_table_lookup (filemap_ht, temp);
	if(!file) { //First encounter
		file  = (struct bin_file *) malloc (sizeof(*file));
		if(!file) {
			monitor_printf(default_mon, "malloc failed in wl_load_module_notify. :'(\n");
			vm_stop(0);
		}
		memset(file, 0, sizeof(*file));
		strcpy(file->name, temp);
		convert_to_host_filename(fullname, host_filename);
		ret = enum_exp_table_reloc_table_to_wl (host_filename, base, name, file);
		if(ret == -1) { //Unable to open file or invalid pe header. Try defaults
			strcpy(name_lower, name);
			strcpy(host_filename, C_DRIVE);
			strcat(host_filename, "\\windows\\system32\\");

			for(i = 0; i < strlen(name); i++)
				name_lower[i] = tolower(name[i]);

			strcat(host_filename, name_lower);
			for(i = 0; i < strlen(host_filename); i++)
				if(host_filename[i] == '\\')
					host_filename[i] = '/';

			ret = enum_exp_table_reloc_table_to_wl(host_filename, base, name, file);
			if(ret == -1) { //might be a driver??
				strcpy(name_lower, name);
				strcpy(host_filename, C_DRIVE);
				strcat(host_filename, "\\windows\\system32\\drivers\\");

				for(i = 0; i < strlen(name); i++)
					name_lower[i] = tolower(name[i]);

				strcat(host_filename, name_lower);
				for(i = 0; i < strlen(host_filename); i++)
					if(host_filename[i] == '\\')
						host_filename[i] = '/';
				ret = enum_exp_table_reloc_table_to_wl(host_filename, base, name, file);
			}
		}
		monitor_printf(default_mon, "%d entries loaded from %s. ", ret, fullname);
	}

	if(file->whitelist == 0) { //Not initialized
		file->whitelist = g_hash_table_new(0,0);
		for(i = 0; i < file->reloc_tbl_count; i++)
			g_hash_table_insert(file->whitelist, (file->reloc_tbl)[i], 1);
		for(i = 0; i < file->exp_tbl_count; i++)
			g_hash_table_insert(file->whitelist, (file->exp_tbl)[i], 2);

		monitor_printf(default_mon, "%d elements in ht.\n", g_hash_table_size(file->whitelist));
	}

	insert_file_to_proc(p, file, base);
	add_proc_module(p, base, size);

	//Free up the reloc table and the export table.
	if(file->exp_tbl) {
		free(file->exp_tbl);
		file->exp_tbl = 0;
	}

	if(file->reloc_tbl) {
		free(file->reloc_tbl);
		file->reloc_tbl = 0;
	}

	WL_cleanUp();

	if(!g_hash_table_lookup (filemap_ht, temp)) {
		if(file->reloc_tbl_count > 0 || file->exp_tbl_count > 0) {
			monitor_printf(default_mon, "Inserting to filemap temp: %s, fullname: %s\n", temp, fullname);
			g_hash_table_insert(filemap_ht, temp, (gpointer)file);
		}
	}

done:
	return;
}

static void wl_procexit(uint32_t pid, uint32_t cr3, char *name)
{
	int i;
	monitor_printf(default_mon, "Process exiting: %s, pid: %d, cr3: %08x\n", name, pid, cr3);
	struct proc_entry *p = (struct proc_entry *) g_hash_table_lookup(cr3_pe_ht, (gconstpointer) cr3);
	if(p == NULL) {
		monitor_printf(default_mon, "wl_procexit(pid = %d, cr3 = 0x%08x, name = %s):Process not present... Stopping VM...\n",
				pid, cr3, name);
		vm_stop(0);
		goto done;
	}
	for(i = 0; i < MAX_THREADS; i++) {
		if(p->threads[i]) {
			if(p->threads[i]->kstack) {
				if(p->threads[i]->kstack->data) {
					free(p->threads[i]->kstack->data);
					p->threads[i]->kstack->data = NULL;
				}
				g_hash_table_destroy(p->threads[i]->kstack->ht);
				p->threads[i]->kstack->ht = NULL;
				free(p->threads[i]->kstack);
				p->threads[i]->kstack = NULL;
			}
			if(p->threads[i]->ustack) {
				if(p->threads[i]->ustack->data) {
					free(p->threads[i]->ustack->data);
					p->threads[i]->ustack->data = NULL;
				}
				g_hash_table_destroy(p->threads[i]->ustack->ht);
				p->threads[i]->ustack->ht = NULL;
				free(p->threads[i]->ustack);
				p->threads[i]->ustack = NULL;
			}
			free(p->threads[i]);
			p->threads[i] = NULL;
		}
	}
	if(p->mod_hashtable)
		g_hash_table_destroy(p->mod_hashtable);

	if(p->misc_whitelist)
		g_hash_table_destroy(p->misc_whitelist);

	g_hash_table_remove(cr3_pe_ht, cr3);

	free(p);
done:
	return;
}

void wl_systemload_notify(uint32_t cr3, uint32_t kernel_base)
{
	startup_registrations();
	return;
}

int (*comparestring)(const char *, const char*);
plugin_interface_t * init_plugin()
{
  comparestring = strcasecmp;

  wl_interface.plugin_cleanup = cfi_cleanup;
  wl_interface.mon_cmds = wl_term_cmds;
  wl_interface.info_cmds = wl_info_cmds;
  loadmainmodule_notify = wl_loadmainmodule_notify;
  loadmodule_notify = wl_load_module_notify;
  removeproc_notify = wl_procexit;

  wl_init ();
  return &wl_interface;
}

