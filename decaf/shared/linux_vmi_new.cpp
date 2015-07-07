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
* linux_vmi_new.cpp
*
*  Created on: June 26, 2015
* 	Author : Abhishek V B
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
#include "linux_vmi_new.h"
#include "linux_procinfo.h"
#include "linux_readelf.h"
#include "hookapi.h"
#include "function_map.h"
#include "shared/utils/SimpleCallback.h"
#include "linux_readelf.h"

using namespace std;
using namespace std::tr1;

#define BREAK_IF(x) if(x) break

//Global variable used to read values from the stack
uint32_t call_stack[12];
int monitored = 0;
static int first = 1;

// current linux profile
static ProcInfo OFFSET_PROFILE = {"VMI"};

//  Traverse the task_struct linked list and add all un-added processes
//  This function is called
static void traverse_task_struct_add(CPUState *env)
{
    uint32_t task_pid = 0;
    const int MAX_LOOP_COUNT = 1024;	// prevent infinite loop
    target_ulong next_task, mm, proc_cr3, task_pgd, ts_parent_pid, ts_real_parent;
    next_task = OFFSET_PROFILE.init_task_addr;

    for (int count = MAX_LOOP_COUNT; count > 0; --count)
    {
        BREAK_IF(DECAF_read_ptr(env, next_task + (OFFSET_PROFILE.ts_tasks + sizeof(target_ptr)),
                                &next_task) < 0);

        next_task -= OFFSET_PROFILE.ts_tasks;


        if(OFFSET_PROFILE.init_task_addr == next_task)
        {
            break;
        }

        BREAK_IF(DECAF_read_ptr(env, next_task + OFFSET_PROFILE.ts_mm,
                                &mm) < 0);

        if (mm != 0)
        {
            BREAK_IF(DECAF_read_ptr(env, mm + OFFSET_PROFILE.mm_pgd,
                                    &task_pgd) < 0);

            proc_cr3 = DECAF_get_phys_addr(env, task_pgd);
        }
        else
        {
            // We don't add kernel processed for now.
            proc_cr3 = -1;
            continue;
        }

        if (!VMI_find_process_by_pgd(proc_cr3))
        {

            // get task_pid
            BREAK_IF(DECAF_read_ptr(env, next_task + OFFSET_PROFILE.ts_tgid,
                                    &task_pid) < 0);

            // get parent task's base address
            BREAK_IF(DECAF_read_ptr(env, next_task + OFFSET_PROFILE.ts_real_parent,
                                    &ts_real_parent) < 0
                     ||
                     DECAF_read_ptr(env, ts_real_parent + OFFSET_PROFILE.ts_tgid,
                                    &ts_parent_pid) < 0);

            process* pe = new process();
            pe->pid = task_pid;
            pe->parent_pid = ts_parent_pid;
            pe->cr3 = proc_cr3;
            pe->EPROC_base_addr = next_task; // store current task_struct's base address
            BREAK_IF(DECAF_read_mem(env, next_task + OFFSET_PROFILE.ts_comm,
                                    SIZEOF_COMM, pe->name) < 0);
            VMI_create_process(pe);
			pe->modules_extracted = false;
        }
    }
}

// Traverse the task_struct linked list and updates the internal DECAF process data structures on process exit
// This is called when the linux system call `proc_exit_connector` is called.
static process *traverse_task_struct_remove(CPUState *env)
{
    set<target_ulong> pids;
    uint32_t task_pid = 0;
    process *right_proc = NULL;
    uint32_t right_pid = 0;

    target_ulong next_task, mm;
    next_task = OFFSET_PROFILE.init_task_addr;

    while(true)
    {
        BREAK_IF(DECAF_read_ptr(env, next_task + (OFFSET_PROFILE.ts_tasks + sizeof(target_ptr)),
                                &next_task) < 0);

        next_task -= OFFSET_PROFILE.ts_tasks;

        if(OFFSET_PROFILE.init_task_addr == next_task)
        {
            break;
        }

        BREAK_IF(DECAF_read_ptr(env, next_task + OFFSET_PROFILE.ts_mm,
                                &mm) < 0);

        if (mm != 0)
        {
            DECAF_read_ptr(env, next_task + OFFSET_PROFILE.ts_tgid,
                           &task_pid);
            // Collect PIDs of all processes in the task linked list
            pids.insert(task_pid);
        }

    }

    // Compare the collected list with the internal list. We track the Process which is removed and call `VMI_process_remove`
    for(unordered_map < uint32_t, process * >::iterator iter = process_pid_map.begin(); iter != process_pid_map.end(); ++iter)
    {
        if(!pids.count(iter->first))
        {
            right_pid = iter->first;
            right_proc = iter->second;
            break;
        }
    }

    //DEBUG-only
    //if(right_proc != NULL)
        //monitor_printf(default_mon,"process with pid [%08x] %s ended\n",right_pid,right_proc->name);

    VMI_remove_process(right_pid);
    return right_proc;
}

// Traverse the memory map for a process
void traverse_mmap(CPUState *env, void *opaque)
{
    process *proc = (process *)opaque;
    target_ulong mm, vma_curr, vma_file, f_dentry, f_inode, mm_mmap, vma_next=NULL;
    set<target_ulong> module_bases;
    unsigned int inode_number;
    target_ulong vma_vm_start = 0, vma_vm_end = 0;
    target_ulong last_vm_start = 0, last_vm_end = 0, mod_vm_start = 0;
    char name[32];	// module file path
    string last_mod_name;
    module *mod = NULL;

    if (DECAF_read_mem(env, proc->EPROC_base_addr + OFFSET_PROFILE.ts_mm, sizeof(target_ptr), &mm) < 0)
        return;

    if (DECAF_read_mem(env, mm + OFFSET_PROFILE.mm_mmap, sizeof(target_ptr), &mm_mmap) < 0)
        return;

    // Mark the `modules_extracted` true. This is done because this function calls `VMI_find_module_by_base`
    // and that function calls `traverse_mmap` if `modules_extracted` is false. We don't want to get into
    // an infinite recursion.
    proc->modules_extracted = true;

    if (-1UL == proc->cr3)
        return;


    // starting from the first vm_area, read vm_file. NOTICE vm_area_struct can be null
    if (( vma_curr = mm_mmap) == 0)
        return;


    while(true)
    {
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


        if (!strcmp(last_mod_name.c_str(), name))
        {
            // extending the module
            if(last_vm_end == vma_vm_start)
            {
                assert(mod);
                target_ulong new_size = vma_vm_end - mod_vm_start;
                if (mod->size < new_size)
                    mod->size = new_size;
            }
            // This is a special case when the data struct is BEING populated
            goto next;
        }

        char key[32+32];
        //not extending, a different module
        mod_vm_start = vma_vm_start;

        sprintf(key, "%u_%s", inode_number, name);
        mod = VMI_find_module_by_key(key);
        module_bases.insert(vma_vm_start);
        if (!mod)
        {
            mod = new module();
            strncpy(mod->name, name, 31);
            mod->name[31] = '\0';
            mod->size = vma_vm_end - vma_vm_start;
            mod->inode_number = inode_number;
            mod->symbols_extracted = 0;
            VMI_add_module(mod, key);
        }

        if(VMI_find_module_by_base(proc->cr3, vma_vm_start) != mod)
        {
            VMI_insert_module(proc->pid, mod_vm_start , mod);
        }

next:
        if (DECAF_read_mem(env, vma_curr + OFFSET_PROFILE.vma_vm_next, sizeof(target_ptr), &vma_next) < 0)
            break;

        if (vma_next==NULL )
        {
            break;
        }

        vma_curr=vma_next;
        last_mod_name=name;
        if (mod != NULL)
        {
            last_vm_start = vma_vm_start;
            last_vm_end = vma_vm_end;
        }
    }


    unordered_map<uint32_t, module *>::iterator iter = proc->module_list.begin();
    set<target_ulong> bases_to_remove;
    for(; iter!=proc->module_list.end(); iter++)
    {
        //DEBUG-only
        //monitor_printf(default_mon,"module %s base %08x \n",iter->second->name,iter->first);
        if (module_bases.find(iter->first) == module_bases.end())
            bases_to_remove.insert(iter->first);
    }

    set<target_ulong>::iterator iter2;
    for (iter2=bases_to_remove.begin(); iter2!=bases_to_remove.end(); iter2++)
    {
        VMI_remove_module(proc->pid, *iter2);
    }
}

//New process callback function
static void new_proc_callback(DECAF_Callback_Params* params)
{
    CPUState *env = params->bb.env;
    target_ulong pc = DECAF_getPC(env);

    if(OFFSET_PROFILE.proc_exec_connector != pc)
        return;

    traverse_task_struct_add(env);
}

//Process exit callback function
static void proc_end_callback(DECAF_Callback_Params *params)
{
    CPUState *env = params->bb.env;

    target_ulong pc = DECAF_getPC(env);

    if(OFFSET_PROFILE.proc_exit_connector != pc)
        return;

    traverse_task_struct_remove(env);
}

// Callback corresponding to `vma_link`,`vma_adjust` & `remove_vma`
// This marks the `modules_extracted` for the process `false`
void VMA_update_func_callback(DECAF_Callback_Params *params)
{
    CPUState *env = params->bb.env;

    target_ulong pc = DECAF_getPC(env);

    if(!(pc == OFFSET_PROFILE.vma_link) && !(pc == OFFSET_PROFILE.vma_adjust) && !(pc == OFFSET_PROFILE.remove_vma))
        return;

    uint32_t pgd =  DECAF_getPGD(env);
    process *proc = NULL;

    proc = VMI_find_process_by_pgd(pgd);

    if(proc)
        proc->modules_extracted = false;
}

// TLB miss callback
// This callback is only used for updating modules when users have registered for either a
// module loaded/unloaded callback.
void Linux_tlb_call_back(DECAF_Callback_Params *temp)
{
    CPUState *ourenv = temp->tx.env;
    uint32_t pgd = -1;
    process *proc = NULL;

    // Check too see if any callbacks are registered
    if(!VMI_is_MoudleExtract_Required())
    {
        return;
    }

    // The first time we register for some VMA related callbacks
    if(first)
    {
        monitor_printf(default_mon,"Registered for VMA update callbacks!\n");
        DECAF_registerOptimizedBlockBeginCallback(&VMA_update_func_callback, NULL, OFFSET_PROFILE.vma_adjust, OCB_CONST);
        DECAF_registerOptimizedBlockBeginCallback(&VMA_update_func_callback, NULL, OFFSET_PROFILE.vma_link, OCB_CONST);
        DECAF_registerOptimizedBlockBeginCallback(&VMA_update_func_callback, NULL, OFFSET_PROFILE.remove_vma, OCB_CONST);
        first = 0;
    }

    pgd = DECAF_getPGD(ourenv);
    proc = VMI_find_process_by_pgd(pgd);

    // Traverse memory map for a process if required.
    if (proc && !proc->modules_extracted)
    {
        traverse_mmap(ourenv, proc);
    }
}


// to see whether this is a Linux or not,
// the trick is to check the init_thread_info, init_task
int find_linux(CPUState *env, uintptr_t insn_handle)
{
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



    if(0 != load_proc_info(env, _thread_info, OFFSET_PROFILE))
    {
        return 0;
    }

    monitor_printf(default_mon, "swapper task @ [%08x] \n", OFFSET_PROFILE.init_task_addr);

    VMI_guest_kernel_base = 0xc0000000;

    return (1);
}



// when we know this is a linux
void linux_vmi_init()
{
    DECAF_registerOptimizedBlockBeginCallback(&new_proc_callback, NULL, OFFSET_PROFILE.proc_exec_connector, OCB_CONST);
    DECAF_registerOptimizedBlockBeginCallback(&proc_end_callback, NULL, OFFSET_PROFILE.proc_exit_connector, OCB_CONST);
    DECAF_register_callback(DECAF_TLB_EXEC_CB, Linux_tlb_call_back, NULL);
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
