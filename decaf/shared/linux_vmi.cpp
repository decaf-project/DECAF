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
/*
 * linux_vmi.cpp
 *
 *  Created on: June 7, 2013
 *      Author: Kevin Wang, Heng Yin
 */

#include <inttypes.h>
#include <string>
#include <list>
#include <set>

#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <queue>
#include <sys/time.h>
#include <math.h>
#include <glib.h>
#include <mcheck.h>
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "cpu.h"
#include "config.h"
#include "hw/hw.h" // AWH
#include "qemu-timer.h"
#ifdef __cplusplus
};
#endif /* __cplusplus */

#include "DECAF_main.h"
#include "DECAF_target.h"
#include "vmi.h"
#include "linux_vmi.h"
#include "linux_procinfo.h"
#include "linux_readelf.h"
#include "hookapi.h"
#include "function_map.h"
#include "shared/utils/SimpleCallback.h"
#include "linux_readelf.h"

using namespace std;
using namespace std::tr1;

#define BREAK_IF(x) if(x) break

#if defined(TARGET_I386)
#define get_new_modules get_new_modules_x86
#elif defined(TARGET_ARM)
#define get_new_modules get_new_modules_arm
#elif defined(TARGET_MIPS)
#define get_new_modules get_new_modules_mips
#else
#error Unknown target
#endif

// current linux profile
static ProcInfo OFFSET_PROFILE = {"VMI"};

// _last_task_pid is used for reducing the memory reading process, when we trying to find a new process, the pid should
// be keeping growing by increment of 1, thus by tracking the pid of the last new process we found, we can speed up the
// process of finding new process. It is based on the fact that the task list is ordered by pid.
// NOTICE: one dangerous thing for this that when the pid is bigger than the "pid max limit", which is 32768 by default,
// this optimization will not work
//static uint32_t last_task_pid = 0;	// last detected task's pid (tgid)

//static process *kernel_proc = NULL;

/* Timer to check for proc exits */
static QEMUTimer *recon_timer = NULL;



// query if one vm page is resolved
static inline bool is_vm_page_resolved(process *proc, uint32_t addr)
{
	return (proc->resolved_pages.find(addr >> 12) != proc->resolved_pages.end());
}

static inline int unresolved_attempt(process *proc, uint32_t addr)
{
	unordered_map <uint32_t, int>::iterator iter = proc->unresolved_pages.find(addr>>12);
	if(iter == proc->unresolved_pages.end()) {
		proc->unresolved_pages[addr>>12] = 1;
		return 1;
	}
	iter->second++;

	return iter->second;
}


void extract_symbols_info(CPUState *env, uint32_t cr3, target_ulong start_addr, module * mod,char* proc_name)
{
	#if 0
 	mod->symbols_extracted = read_elf_info(env, cr3, mod->name, start_addr, mod->size, mod->inode_number);
	#endif
}

// get new module, basically reading from mm_struct
static void get_new_modules_x86(CPUState* env, process * proc)
{	

	set<target_ulong> module_bases;
	

	target_ulong ts_mm, mm_mmap, vma_curr, vma_file, f_dentry, f_inode, vma_next=NULL;
	unsigned int inode_number;
	const int MAX_LOOP_COUNT = 1024;	// prevent infinite loop
	target_ulong vma_vm_start = 0, vma_vm_end = 0;
	target_ulong last_vm_start = 0, last_vm_end = 0, mod_vm_start = 0;
	char name[32];	// module file path
	char key[32+32];
	string last_mod_name, mod_name;
	module *mod = NULL;
	
	bool finished_traversal = false;
	

	// quit extracting modules when this proc doesn't have mm (kernel thread, etc.)
	if ( !proc || -1UL == proc->cr3
		|| DECAF_read_mem(env, proc->EPROC_base_addr + OFFSET_PROFILE.ts_mm, sizeof(target_ptr), &ts_mm) < 0
		|| ts_mm < 0)
		return;

	// read vma from mm first, then traverse mmap
	if (DECAF_read_mem(env, ts_mm + OFFSET_PROFILE.mm_mmap, sizeof(target_ptr), &mm_mmap) < 0)
		return;

	// starting from the first vm_area, read vm_file. NOTICE vm_area_struct can be null
	if (( vma_curr = mm_mmap) == 0)
		return;

// AVB, changed logic of module updation
	for (size_t count = MAX_LOOP_COUNT; count--; ) {

			
		// read start of curr vma
		if (DECAF_read_mem(env, vma_curr + OFFSET_PROFILE.vma_vm_start, sizeof(target_ptr), &vma_vm_start) < 0)
			goto next;

		// read end of curr vma
		if (DECAF_read_mem(env, vma_curr + OFFSET_PROFILE.vma_vm_end, sizeof(target_ptr), &vma_vm_end) < 0)
			goto next;

		// read the struct* file entry of the curr vma, used to then extract the dentry of the this page
		if (DECAF_read_mem(env, vma_curr + OFFSET_PROFILE.vma_vm_file, sizeof(target_ptr), &vma_file) < 0 || !vma_file)
			goto next;
		
		// dentry extraction from the struct* file
		if (DECAF_read_mem(env, vma_file + OFFSET_PROFILE.file_dentry, sizeof(target_ptr), &f_dentry) < 0 || !f_dentry)
			goto next;

	
		// read small names form the dentry
		if (DECAF_read_mem(env, f_dentry + OFFSET_PROFILE.dentry_d_iname, 32, name) < 0)
			goto next;

		
		// inode struct extraction from the struct* file
		if (DECAF_read_mem(env, f_dentry + OFFSET_PROFILE.file_inode, sizeof(target_ptr), &f_inode) < 0 || !f_inode)
			goto next;

		// inode_number extraction
		if (DECAF_read_mem(env, f_inode + OFFSET_PROFILE.inode_ino , sizeof(unsigned int), &inode_number) < 0 || !inode_number)
			goto next;
				
		name[31] = '\0';	// truncate long string

		
		// name is invalid, move on the data structure
		if (strlen(name)==0)
			goto next;


		
		if (last_vm_end == vma_vm_start && !strcmp(last_mod_name.c_str(), name)) { 
			// extending the module
			assert(mod);

			target_ulong new_size = vma_vm_end - mod_vm_start;
			if (mod->size < new_size)
				mod->size = new_size;
			goto next;
		}

		//not extending, a different module
		mod_vm_start = vma_vm_start;
		sprintf(key, "%u_%s", inode_number, name);
		mod = VMI_find_module_by_key(key);
		if (!mod) {
			mod = new module();
			strncpy(mod->name, name, 31);
			mod->name[31] = '\0';
			mod->size = vma_vm_end - vma_vm_start;
			mod->inode_number = inode_number;
			mod->symbols_extracted = 0;
			VMI_add_module(mod, key);
			module_bases.insert(vma_vm_start);
		}
	
		if(VMI_find_module_by_base(proc->cr3, mod_vm_start) != mod) {
			VMI_insert_module(proc->pid, mod_vm_start , mod);
		}
		
		

next:	if (DECAF_read_mem(env, vma_curr + OFFSET_PROFILE.vma_vm_next, sizeof(target_ptr), &vma_next) < 0)
			break;

		if (vma_next==NULL ) {
			finished_traversal = true;
			break;
		}

		vma_curr=vma_next;
		last_mod_name=name;
		if (mod != NULL) {
			last_vm_start = vma_vm_start;
			last_vm_end = vma_vm_end;
		}
	}

	

	if (finished_traversal) {
		unordered_map<uint32_t, module *>::iterator iter = proc->module_list.begin();
		set<target_ulong> bases_to_remove;
		for(; iter!=proc->module_list.end(); iter++) {
			if (module_bases.find(iter->first) == module_bases.end())
				bases_to_remove.insert(iter->first);
		}

		set<target_ulong>::iterator iter2;
		for (iter2=bases_to_remove.begin(); iter2!=bases_to_remove.end(); iter2++) {
			if (proc->pid == 1)
				//monitor_printf(default_mon, "removed module %08x\n", *iter2);

			VMI_remove_module(proc->pid, *iter2);

		}
	}
}

// get new modules for arm, remains to be improved
static void get_new_modules_arm(CPUState* env, process * proc)
{
	target_ulong ts_mm, mm_mmap, vma_file, vma_next, f_dentry;
	const int MAX_LOOP_COUNT = 1024;	// prevent infinite loop
	target_ulong vma_vm_start = 0, vma_vm_end = 0, vma_vm_flags, vma_vm_pgoff;
	target_ulong last_vm_start = 0, last_vm_end = 0;
	char name[32], key[128];	// module file path
	string last_mod_name, mod_name;
	target_ulong mod_vm_start, mod_vm_end;
	module* mod = NULL;
	string _name;
	set<uint32_t> module_bases;
	bool finished_traversal = false;
	int mod_stage = 0;
	bool two_sections_found = false;
	static int offset_populated = 0, dentry_offset_populated = 0;
	const int VM_FLAGS_RX = 5;
	const int VM_FLAGS_NONE = 0;
	const int VM_FLAGS_RWX = 7; 

	// quit extracting modules when this proc doesn't have mm (kernel thread, etc.)
	if ( !proc || -1UL == proc->cr3
		|| DECAF_read_mem(env, proc->EPROC_base_addr + OFFSET_PROFILE.ts_mm, sizeof(target_ptr), &ts_mm) < 0
		|| ts_mm < 0)
		return;

	// read vma from mm first, then traverse mmap
	if (DECAF_read_mem(env, ts_mm + OFFSET_PROFILE.mm_mmap, sizeof(target_ptr), &mm_mmap) < 0)
		return;

	// starting from the first vm_area, read vm_file. NOTICE vm_area_struct can be null
	if ((vma_next = mm_mmap) == 0)
		return;

	// // see if vm_area is populated already
	// if (populate_vm_area_struct_offsets(env, vma_next, &OFFSET_PROFILE))
	// 	return;

	for (size_t count = MAX_LOOP_COUNT; count--; ) {

		// read current vma's size
		if (DECAF_read_mem(env, vma_next + OFFSET_PROFILE.vma_vm_start, sizeof(target_ptr), &vma_vm_start) < 0)
			goto next;

		if (DECAF_read_mem(env, vma_next + OFFSET_PROFILE.vma_vm_end, sizeof(target_ptr), &vma_vm_end) < 0)
			goto next;

		if (DECAF_read_mem(env, vma_next + OFFSET_PROFILE.vma_vm_file, sizeof(target_ptr), &vma_file) < 0 || !vma_file)
			goto next;

		// if (!offset_populated && (offset_populated = !getDentryFromFile(env, vma_file, &OFFSET_PROFILE)))	// populate dentry offset
		// 	goto next;


		if (DECAF_read_mem(env, vma_file + OFFSET_PROFILE.file_dentry, sizeof(target_ptr), &f_dentry) < 0 || !f_dentry)
			goto next;

		// if (!dentry_offset_populated && (dentry_offset_populated = !populate_dentry_struct_offsets(env, f_dentry, &OFFSET_PROFILE)))
		// 	goto next;	// notice this time we are not going to reset mark-bit. all plugins are populated by far


		if (DECAF_read_mem(env, vma_next + OFFSET_PROFILE.vma_vm_flags, sizeof(target_ulong), &vma_vm_flags) < 0)
			goto next;

		if (DECAF_read_mem(env, vma_next + OFFSET_PROFILE.vma_vm_pgoff, sizeof(target_ulong), &vma_vm_pgoff) < 0)
			goto next;

		// read small names
		if (DECAF_read_mem(env, f_dentry + OFFSET_PROFILE.dentry_d_iname, 32, name) < 0)
			goto next;
			
		name[31] = '\0';	// truncate long string

		switch(mod_stage) {
		case 0:
			//READ + EXECUTE
			if (VM_FLAGS_RX == (vma_vm_flags & 0xf) && /*vma_vm_pgoff == 0 &&*/ strlen(name)) {
				mod_stage = 1;
				mod_name = name;
				mod_vm_start = vma_vm_start;
				mod_vm_end = vma_vm_end;
			}
			break;

		case 1:
			if (VM_FLAGS_RX ==  (vma_vm_flags & 0xf) && /*vma_vm_pgoff == 0 && */strlen(name)) {
				mod_stage = 1;
				mod_name = name;
				mod_vm_start = vma_vm_start;
				mod_vm_end = vma_vm_end;
			} else if (VM_FLAGS_RWX == (vma_vm_flags & 0xf) && !mod_name.compare(name)
					&& vma_vm_start == mod_vm_end
					&& vma_vm_pgoff != 0 ) {
				//Now we have seen all two sections in order
				//We can insert the module now.
				mod_vm_end = vma_vm_end;
				two_sections_found = true;
				mod_stage = 0;
			} 
			else if(VM_FLAGS_NONE == (vma_vm_flags & 0xf) && !mod_name.compare(name)
					&& vma_vm_start == mod_vm_end
					&& vma_vm_pgoff != 0) {
				mod_stage = 1;
				mod_vm_end = vma_vm_end;
			} else {
				mod_stage = 0;
			}
			break;

		default:
			assert(0); break;
		}

		if (!two_sections_found)
			goto next;

		two_sections_found = false;

		mod = VMI_find_module_by_key(mod_name.c_str());
		if (!mod) {
			mod = new module();
			strncpy(mod->name, mod_name.c_str(), 31);
			mod->name[31] = '\0';
			mod->size = mod_vm_end - mod_vm_start;
			VMI_add_module(mod, mod_name.c_str());
		}

		module_bases.insert(mod_vm_start);

		if(VMI_find_module_by_base(proc->cr3, mod_vm_start) != mod) {
			VMI_insert_module(proc->pid, mod_vm_start, mod);
			//if (proc->pid == 1)
			// monitor_printf(default_mon, "Module (%s, 0x%08x->0x%08x, size %u) is loaded to proc %s (pid = %d) \n",
			//			mod_name.c_str(), mod_vm_start, mod_vm_end, mod->size / 1024, proc->name, proc->pid);
		}
next:
		if (DECAF_read_mem(env, vma_next + OFFSET_PROFILE.vma_vm_next, sizeof(target_ptr), &vma_next) < 0)
			break;
		if (!vma_next || vma_next == mm_mmap) {
			finished_traversal = true;
			break;
		}
		
	}

	if (finished_traversal) {
		unordered_map<uint32_t, module *>::iterator iter = proc->module_list.begin();
		set<uint32_t> bases_to_remove;
		for(; iter!=proc->module_list.end(); iter++) {
			if (module_bases.find(iter->first) == module_bases.end())
				bases_to_remove.insert(iter->first);
		}

		set<uint32_t>::iterator iter2;
		for (iter2=bases_to_remove.begin(); iter2!=bases_to_remove.end(); iter2++) {
			if (proc->pid == 1)
				monitor_printf(default_mon, "removed module %08x\n", *iter2);

			VMI_remove_module(proc->pid, *iter2);

		}

	}
}

// void get_new_modules_mips(CPUState* env, process * proc) __attribute__((optimize("O0")));
static
void get_new_modules_mips(CPUState* env, process * proc)
{
	target_ulong ts_mm, mm_mmap, vma_file, vma_next, f_dentry;
	const int MAX_LOOP_COUNT = 1024;	// prevent infinite loop
	target_ulong vma_vm_start = 0, vma_vm_end = 0, vma_vm_flags, vma_vm_pgoff;
	target_ulong last_vm_start = 0, last_vm_end = 0;
	char name[32], key[128];	// module file path
	string last_mod_name, mod_name;
	target_ulong mod_vm_start, mod_vm_end;
	module* mod = NULL;
	string _name;
	set<uint32_t> module_bases;
	bool finished_traversal = false;
	int mod_stage = 0;
	bool three_sections_found = false;
	static int offset_populated = 0, dentry_offset_populated = 0;
	const int VM_FLAGS_RX = 5;
	const int VM_FLAGS_NONE = 0;
	const int VM_FLAGS_R = 1;
	const int VM_FLAGS_RWX = 7; 
	const int VM_FLAGS_RW = 3; 
	// puts("here");
	// quit extracting modules when this proc doesn't have mm (kernel thread, etc.)
	if ( !proc || -1UL == proc->cr3
		|| DECAF_read_ptr(env, proc->EPROC_base_addr + OFFSET_PROFILE.ts_mm, &ts_mm) < 0
		|| ts_mm < 0)
		return;

	// read vma from mm first, then traverse mmap
	if (DECAF_read_ptr(env, ts_mm + OFFSET_PROFILE.mm_mmap, &mm_mmap) < 0)
		return;

	// starting from the first vm_area, read vm_file. NOTICE vm_area_struct can be null
	if ((vma_next = mm_mmap) == 0)
		return;

	// // see if vm_area is populated already
	// if (populate_vm_area_struct_offsets(env, vma_next, &OFFSET_PROFILE))
	// 	return;

	for (size_t count = MAX_LOOP_COUNT; count--; ) {

		// read current vma's size
		if (DECAF_read_ptr(env, vma_next + OFFSET_PROFILE.vma_vm_start, &vma_vm_start) < 0)
			goto next;

		if (DECAF_read_ptr(env, vma_next + OFFSET_PROFILE.vma_vm_end, &vma_vm_end) < 0)
			goto next;

		if (DECAF_read_ptr(env, vma_next + OFFSET_PROFILE.vma_vm_file, &vma_file) < 0 || !vma_file)
			goto next;

		// if (!offset_populated && (offset_populated = !getDentryFromFile(env, vma_file, &OFFSET_PROFILE)))	// populate dentry offset
		// 	goto next;


		if (DECAF_read_ptr(env, vma_file + OFFSET_PROFILE.file_dentry, &f_dentry) < 0 || !f_dentry)
			goto next;

		// if (!dentry_offset_populated && (dentry_offset_populated = !populate_dentry_struct_offsets(env, f_dentry, &OFFSET_PROFILE)))
		// 	goto next;	// notice this time we are not going to reset mark-bit. all plugins are populated by far


		if (DECAF_read_ptr(env, vma_next + OFFSET_PROFILE.vma_vm_flags, &vma_vm_flags) < 0)
			goto next;

		if (DECAF_read_ptr(env, vma_next + OFFSET_PROFILE.vma_vm_pgoff, &vma_vm_pgoff) < 0)
			goto next;

		// read small names
		if (DECAF_read_mem(env, f_dentry + OFFSET_PROFILE.dentry_d_iname, 32, name) < 0)
			goto next;
			
		name[31] = '\0';	// truncate long string

		switch(mod_stage) {
		case 0:
			if ((vma_vm_flags & 0xf) == VM_FLAGS_RX && /*vma_vm_pgoff == 0 &&*/ strlen(name)) {
				mod_stage = 1;
				mod_name = name;
				mod_vm_start = vma_vm_start;
				mod_vm_end = vma_vm_end;
			}
			break;

		case 1:
			if ((vma_vm_flags & 0xf) == VM_FLAGS_RX && /*vma_vm_pgoff == 0 && */strlen(name)) {
				mod_stage = 1;
				mod_name = name;
				mod_vm_start = vma_vm_start;
				mod_vm_end = vma_vm_end;
			} //READ ONLY
			else if((vma_vm_flags & 0xf) == VM_FLAGS_R && !mod_name.compare(name)
					// && vma_vm_start == mod_vm_end
					&& vma_vm_pgoff != 0) {
				mod_stage = 2;
				mod_vm_end = vma_vm_end;
			}
			else if((vma_vm_flags & 0xf) == VM_FLAGS_NONE && !mod_name.compare(name))
			{
				mod_stage = 1;
				mod_name = name;
				mod_vm_end = vma_vm_end;
			} else if ((vma_vm_flags & 0xf) == VM_FLAGS_RW && !mod_name.compare(name)
					// && vma_vm_start == mod_vm_end
					&& vma_vm_pgoff != 0 ) {
				//Now we have seen all three sections in order
				//We can insert the module now.
				mod_vm_end = vma_vm_end;
				three_sections_found = true;
				mod_stage = 0;					
			}
			else {
				mod_stage = 0;
			}
			break;

		case 2:
			if ((vma_vm_flags & 0xf) == VM_FLAGS_RX && /*vma_vm_pgoff == 0 &&*/ strlen(name)) {
				mod_stage = 1;
				mod_name = name;
				mod_vm_start = vma_vm_start;
				mod_vm_end = vma_vm_end;
			} else if ((vma_vm_flags & 0xf) == VM_FLAGS_RW && !mod_name.compare(name)
					// && vma_vm_start == mod_vm_end
					&& vma_vm_pgoff != 0 ) {
				//Now we have seen all three sections in order
				//We can insert the module now.
				mod_vm_end = vma_vm_end;
				three_sections_found = true;
				mod_stage = 0;
			} else {
				mod_stage = 0;
			}
			break;

		default:
			assert(0); break;
		}

		if (!three_sections_found)
			goto next;

		three_sections_found = false;

		mod = VMI_find_module_by_key(mod_name.c_str());
		if (!mod) {
			mod = new module();
			strncpy(mod->name, mod_name.c_str(), 31);
			mod->name[31] = '\0';
			mod->size = mod_vm_end - mod_vm_start;
			VMI_add_module(mod, mod_name.c_str());
		}

		module_bases.insert(mod_vm_start);

		if(VMI_find_module_by_base(proc->cr3, mod_vm_start) != mod) {
			VMI_insert_module(proc->pid, mod_vm_start, mod);
			//if (proc->pid == 1)
			// monitor_printf(default_mon, "Module (%s, 0x%08x->0x%08x, size %u) is loaded to proc %s (pid = %d) \n",
			//			mod_name.c_str(), mod_vm_start, mod_vm_end, mod->size / 1024, proc->name, proc->pid);
		}

next:
		if (DECAF_read_ptr(env, vma_next + OFFSET_PROFILE.vma_vm_next, &vma_next) < 0)
			break;
		if (!vma_next || vma_next == mm_mmap) {
			finished_traversal = true;
			break;
		}
	}

	if (finished_traversal) {
		unordered_map<uint32_t, module *>::iterator iter = proc->module_list.begin();
		set<uint32_t> bases_to_remove;
		for(; iter!=proc->module_list.end(); iter++) {
			if (module_bases.find(iter->first) == module_bases.end())
				bases_to_remove.insert(iter->first);
		}

		set<uint32_t>::iterator iter2;
		for (iter2=bases_to_remove.begin(); iter2!=bases_to_remove.end(); iter2++) {
			if (proc->pid == 1)
				monitor_printf(default_mon, "removed module %08x\n", *iter2);

			VMI_remove_module(proc->pid, *iter2);

		}

	}
}



// process * find_new_process(CPUState *env, uint32_t cr3) __attribute__((optimize("O0")));
// scan the task list and find new process
static
process * find_new_process(CPUState *env, uint32_t cr3) {
	uint32_t task_pid = 0, ts_parent_pid = 0, proc_cr3 = -1;
	const int MAX_LOOP_COUNT = 1024; // maximum loop count when trying to find a new process (will there be any?)
	process *right_proc = NULL;

	//static target_ulong _last_next_task = 0;// another way to speed up: when the last task remain the same, return immediately

	//uint32_t _last_task_pid = last_task_pid;
	target_ulong next_task, ts_real_parent, mm, task_pgd;
	next_task = OFFSET_PROFILE.init_task_addr;

	// avoid infinite loop
	for (int count = MAX_LOOP_COUNT; count > 0; --count)
	{

		// NOTICE by reading next_task at the beginning, we are skipping the "swapper" task
		// highly likely linux add the latest process to the tail of the linked list, so we go backward here
		BREAK_IF(DECAF_read_ptr(env, 
			next_task + (OFFSET_PROFILE.ts_tasks + sizeof(target_ptr)),
			&next_task) < 0);

		// NOTE - tasks is a list_head, so we need to minus offset to get the base address
		next_task -= OFFSET_PROFILE.ts_tasks;
/*		if (_last_next_task == next_task
				|| next_task == OFFSET_PROFILE.init_task_addr) {// we have traversed the whole link list, or no new process
			break;
		}*/

		if(OFFSET_PROFILE.init_task_addr == next_task)
		{
			break;
		}

		// read task pid, jump out directly when we fail
		BREAK_IF(DECAF_read_ptr(env,
			next_task + OFFSET_PROFILE.ts_tgid,
			&task_pid) < 0);

		BREAK_IF(DECAF_read_ptr(env,
			next_task + OFFSET_PROFILE.ts_mm,
			&mm) < 0);

		// // NOTICE kernel thread does not own a process address space, thus its mm is NULL. It uses active_mm instead
		// if (populate_mm_struct_offsets(env, mm, &OFFSET_PROFILE))
		// 	continue;	// try until we get it.

		if (mm != 0)
		{ 	// for user-processes
			// we read the value of active_mm into mm here
			BREAK_IF(DECAF_read_ptr(env,
					next_task + OFFSET_PROFILE.ts_mm + sizeof(target_ptr),
					&mm) < 0
					||
					DECAF_read_ptr(env,
					mm + OFFSET_PROFILE.mm_pgd,
					&task_pgd) < 0);

			proc_cr3 = DECAF_get_phys_addr(env, task_pgd);
		}
		else
		{	// for kernel threads
			proc_cr3 = -1;// when proc_cr3 is -1UL, we cannot find the process by findProcessByCR3(), but we still can do findProcessByPid()
		}

		if (!VMI_find_process_by_pgd(proc_cr3)) {
			// get parent task's base address
			BREAK_IF(DECAF_read_ptr(env,
					next_task + OFFSET_PROFILE.ts_real_parent,
					&ts_real_parent) < 0
					||
					DECAF_read_ptr(env,
					ts_real_parent + OFFSET_PROFILE.ts_tgid,
					&ts_parent_pid) < 0);

			process* pe = new process();
			pe->pid = task_pid;
			pe->parent_pid = ts_parent_pid;
			pe->cr3 = proc_cr3;
			pe->EPROC_base_addr = next_task; // store current task_struct's base address
			BREAK_IF(DECAF_read_mem(env,
					next_task + OFFSET_PROFILE.ts_comm,
					SIZEOF_COMM, pe->name) < 0);
			VMI_create_process(pe);

			//monitor_printf(default_mon, "new proc = %s, pid = %d, parent_pid = %d \n", pe->name, pe->pid, pe->parent_pid);
			if (cr3 == proc_cr3) {// for kernel thread, we are going to return NULL
				// NOTICE we may find multiple processes in this function, but we only return the current one
				right_proc = pe;
			}
		}
	}

	//last_task_pid = _last_task_pid;
	
	return right_proc;
}

// retrive symbols from specific process
static void retrive_symbols(CPUState *env, process * proc) {

}


// for every tlb call back, we try finding new processes
// static
// void Linux_tlb_call_back(DECAF_Callback_Params *temp) __attribute__((optimize("O0")));
void Linux_tlb_call_back(DECAF_Callback_Params *temp)
{
	CPUState *ourenv = temp->tx.env;
	uint32_t vaddr = temp->tx.vaddr;
	uint32_t pgd = -1;
	process *proc = NULL;
	bool found_new = false;
	pgd = DECAF_getPGD(ourenv);


	//TODO: kernel modules are not retrieved in the current implementation.
	if (DECAF_is_in_kernel(ourenv)) {
		//proc = kernel_proc;
	}
	else if ( (proc = VMI_find_process_by_pgd(pgd)) == NULL) {
		found_new = ((proc = find_new_process(ourenv, pgd)) != NULL);
	}

	if (proc) {	// we are not scanning modules for kernel threads, since kernel thread's cr3 is -1UL, the proc should be null

		if ( !is_vm_page_resolved(proc, vaddr) ) {
			char task_comm[SIZEOF_COMM];
			if ( !found_new
				&& !DECAF_read_mem(ourenv, proc->EPROC_base_addr + OFFSET_PROFILE.ts_comm, SIZEOF_COMM, task_comm) 
				&& strncmp(proc->name, task_comm, SIZEOF_COMM) ) {
					strcpy(proc->name, task_comm);
					//message_p(proc, '^');
			}

			get_new_modules(ourenv, proc);

			//If this page still cannot be resolved, we give up.
			if (!is_vm_page_resolved(proc, vaddr)) {
				int attempts = unresolved_attempt(proc, vaddr);
				if (attempts > 200)
					proc->resolved_pages.insert(vaddr>>12);
			}
		}

		// retrieve symbol information here
		//retrive_symbols(env, proc);
	}
}

// here we scan the task list in guest OS and sync ours with it
static void check_procexit(void *) {
        /* AWH - cpu_single_env is invalid outside of the main exec thread */
	CPUState *env = /* AWH cpu_single_env ? cpu_single_env :*/ first_cpu;
	qemu_mod_timer(recon_timer,
				   qemu_get_clock_ns(vm_clock) + get_ticks_per_sec() * 10);

	target_ulong next_task = OFFSET_PROFILE.init_task_addr;
	set<target_ulong> live_pids;
	set<target_ulong> vmi_pids;
	set<target_ulong> dead_pids;

	const int MAX_LOOP_COUNT = 1024;

	for(int i=0; i<MAX_LOOP_COUNT; i++)
	{
		target_ulong task_pid;
		BREAK_IF(DECAF_read_ptr(env,
			next_task + OFFSET_PROFILE.ts_tgid, 
			&task_pid) < 0);
		live_pids.insert(task_pid);

		BREAK_IF(DECAF_read_ptr(env,
			next_task + OFFSET_PROFILE.ts_tasks + sizeof(target_ptr),
			&next_task) < 0);

		next_task -= OFFSET_PROFILE.ts_tasks;
		if (next_task == OFFSET_PROFILE.init_task_addr)
		{
			break;
		}
	}

	unordered_map<uint32_t, process *>::iterator iter = process_pid_map.begin();
	for(; iter != process_pid_map.end(); iter++)
	{
		vmi_pids.insert(iter->first);
	}

	set_difference(vmi_pids.begin(), vmi_pids.end(), live_pids.begin(), live_pids.end(),
			inserter(dead_pids, dead_pids.end()));

	set<target_ulong>::iterator iter2;
	for(iter2 = dead_pids.begin(); iter2 != dead_pids.end(); iter2++)
	{
		VMI_remove_process(*iter2);
	}

#if 0
	// now do the cleaning
	if (processes != NULL) {
		// what we do here is to traverse the running task list,
		// remove non-exist tasks
		for (size_t i = numofProc; i--; ) {
			unordered_map<uint32_t, std::string>::iterator ts_it;
			proc = &processes[i];

			if ( (ts_it = tasks.find(proc->pid)) == tasks.end()) {	// remove when not found
				removeProcV(proc->pid);
				message_p_d(proc, '-');
				//monitor_printf(default_mon, "proc %s (pid = %d) is removed \n", proc->name, proc->pid);
			}
			else if ( ts_it->second.compare(proc->name) ) {	// here we also update the new for the whole process list
				//monitor_printf(default_mon, "update proc (%s -> %s), pid = %d \n", proc->name, ts_it->second.c_str(), proc->pid);
				strcpy(proc->name, ts_it->second.c_str());
				message_p_d(proc, '^');
				// for those whose names are not up to date, their modules may not be updated as well
				get_new_modules( env, proc );
				// retrive symbol for them as well
				retrive_symbols( env, proc );
			}
		}
	}
#endif
}

// to see whether this is a Linux or not,
// the trick is to check the init_thread_info, init_task
int find_linux(CPUState *env, uintptr_t insn_handle) {
	target_ulong _thread_info = DECAF_getESP(env) & ~ (guestOS_THREAD_SIZE - 1);
	static target_ulong _last_thread_info = 0;

	// if current address is tested before, save time and do not try it again
	if (_thread_info == _last_thread_info || _thread_info <= 0x80000000)
		return 0;
	// first time run
	if (_last_thread_info == 0)
	{
		// memset(&OFFSET_PROFILE.init_task_addr, -1, sizeof(ProcInfo) - sizeof(OFFSET_PROFILE.strName));
	}

	_last_thread_info = _thread_info;

	// ProcInfo temp_offset_profile;
	// // try populate kernel offset, NOTICE we cannot get mm_struct offset yet
	// if (populate_kernel_offsets(env, _thread_info, &temp_offset_profile) != 0)
	// 	return (0);

	if(0 != load_proc_info(env, _thread_info, OFFSET_PROFILE))
	{
		return 0;
	}
	
	monitor_printf(default_mon, "swapper task @ [%08x] \n", OFFSET_PROFILE.init_task_addr);

	// load library function offset
	load_library_info(OFFSET_PROFILE.strName);

	// load_proc_info(OFFSET_PROFILE, temp_offset_profile.init_task_addr);
	//printProcInfo(&OFFSET_PROFILE);
	VMI_guest_kernel_base = 0xc0000000;

	return (1);
}



// when we know this is a linux
void linux_vmi_init()
{

	DECAF_register_callback(DECAF_TLB_EXEC_CB, Linux_tlb_call_back, NULL);

	recon_timer = qemu_new_timer_ns(vm_clock, check_procexit, 0);
	qemu_mod_timer(recon_timer,
				   qemu_get_clock_ns(vm_clock) + get_ticks_per_sec() * 20);

}


gpa_t mips_get_cur_pgd(CPUState *env)
{
	const target_ulong MIPS_KERNEL_BASE = 0x80000000;
	gpa_t pgd = 0;
	if(0 == OFFSET_PROFILE.mips_pgd_current)
	{
		monitor_printf(default_mon, "Error\nmips_get_cur_pgd: read pgd before procinfo is populated.\n");
		return 0;
	}

	DECAF_read_ptr(env,
					OFFSET_PROFILE.mips_pgd_current,
					&pgd);
	pgd &= ~MIPS_KERNEL_BASE;
	return pgd;
}




