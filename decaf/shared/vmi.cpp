#include <inttypes.h>
#include <string>
#include <list>
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
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
//#include "sqlite3/sqlite3.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "cpu.h"
#include "config.h"
#include "hw/hw.h" // AWH
#include "DECAF_target.h"

#ifdef __cplusplus
};
#endif /* __cplusplus */
#include "windows_vmi.h"
#include "linux_vmi.h"

#include "hookapi.h"
// AWH #include "read_linux.h"
#include "shared/vmi.h"
#include "shared/DECAF_main.h"
#include "shared/hookapi.h"
#include "shared/function_map.h"
#include "shared/utils/SimpleCallback.h"


using namespace std;
using namespace std::tr1;

//map cr3 to process_info_t
unordered_map < uint32_t, process * >process_map;
//map pid to process_info_t
unordered_map < uint32_t, process * >process_pid_map;
// module list
unordered_map < string, module * >module_name;

uint32_t GuestOS_index_c=11;
uintptr_t insn_handle_c = 0;

target_ulong VMI_guest_kernel_base = 0;

static os_handle_c handle_funds_c[] = {
#ifdef TARGET_I386
		{ WINXP_SP2_C, &find_winxpsp2, &win_vmi_init, },
		{ WINXP_SP3_C, &find_winxpsp3, &win_vmi_init, },
		{ WIN7_SP0_C, &find_win7sp0, &win_vmi_init, },
		{ WIN7_SP1_C, &find_win7sp1, &win_vmi_init, },
#endif
		{ LINUX_GENERIC_C, &find_linux, &linux_vmi_init,},
};



static void block_end_cb(DECAF_Callback_Params* temp)
{
    static long long count_out = 0x8000000000L;	// detection fails after 1000 basic blocks
    int found_guest_os = 0;

    // Probing guest OS for every basic block is too expensive and wasteful.
    // Let's just do it once every 256 basic blocks
    if ((count_out & 0xff) != 0)
    	goto _skip_probe;

	for(size_t i=0; i<sizeof(handle_funds_c)/sizeof(handle_funds_c[0]); i++)
	{
		if(handle_funds_c[i].find(temp->ie.env, insn_handle_c) == 1)
		{
			GuestOS_index_c = i;
			found_guest_os = 1;
		}
	}

#ifdef TARGET_I386
	if(GuestOS_index_c == 0 || GuestOS_index_c == 1)
		monitor_printf(default_mon, "its win xp \n");
	else if(GuestOS_index_c == 2 || GuestOS_index_c == 3)
		monitor_printf(default_mon, "its win 7 \n");
	else if(GuestOS_index_c == 4)
		monitor_printf(default_mon, "its linux \n");
#endif

	if(found_guest_os)
	{
		DECAF_unregister_callback(DECAF_TLB_EXEC_CB, insn_handle_c);
		handle_funds_c[GuestOS_index_c].init();
	}

_skip_probe:

	if (count_out-- <= 0)	{// does not find
		DECAF_unregister_callback(DECAF_TLB_EXEC_CB, insn_handle_c);
		monitor_printf(default_mon, "oops! guest OS type cannot be decided. \n");
	}
}


process* VMI_find_process_by_name(const char *name)
{
	unordered_map < uint32_t, process * >::iterator iter;
	for (iter = process_map.begin(); iter != process_map.end(); iter++) {
		process * proc = iter->second;
		if (strcmp((const char *)name,proc->name) == 0) {
			return proc;
		}
	}
	return 0;
}

process * VMI_find_process_by_pgd(uint32_t pgd)
{
    unordered_map < uint32_t, process * >::iterator iter =
	process_map.find(pgd);

    if (iter != process_map.end())
		return iter->second;

	return NULL;
}


process *VMI_find_process_by_pid(uint32_t pid)
{
	unordered_map < uint32_t, process * >::iterator iter =
		process_pid_map.find(pid);

	if (iter == process_pid_map.end())
		return NULL; 

	return iter->second;
}


module* VMI_find_module_by_key(const char *key)
{
	string temp(key);
	unordered_map < string, module * >::iterator iter =
		module_name.find(temp);
	if (iter != module_name.end()){
		return iter->second;
	}
	return NULL;
}

module * VMI_find_module_by_base(target_ulong pgd, uint32_t base)
{
	unordered_map<uint32_t, process *>::iterator iter = process_map.find(pgd);
	process *proc;

	if (iter == process_pid_map.end()) //pid not found
		return NULL;

	proc = iter->second;

	unordered_map<uint32_t, module *>::iterator iter_m = proc->module_list.find(base);
	if(iter_m == proc->module_list.end())
		return NULL;

	return iter_m->second;
}

module * VMI_find_module_by_pc(target_ulong pc, target_ulong pgd, target_ulong *base)
{
	process *proc ;
	if (pc >= VMI_guest_kernel_base) {
		proc = process_pid_map[0];
	} else {
		unordered_map < uint32_t, process * >::iterator iter_p = process_map.find(pgd);
		if (iter_p == process_map.end())
			return NULL;

		proc = iter_p->second;
	}

	unordered_map< uint32_t, module * >::iterator iter;
	for (iter = proc->module_list.begin(); iter != proc->module_list.end(); iter++) {
		module *mod = iter->second;
		if (iter->first <= pc && mod->size + iter->first > pc) {
			*base = iter->first;
			return mod;
		}
	}

    return NULL;
}

module * VMI_find_module_by_name(const char *name, target_ulong pgd, target_ulong *base)
{
	unordered_map < uint32_t, process * >::iterator iter_p = process_map.find(pgd);
	if (iter_p == process_map.end())
		return NULL;

	process *proc = iter_p->second;

	unordered_map< uint32_t, module * >::iterator iter;
	for (iter = proc->module_list.begin(); iter != proc->module_list.end(); iter++) {
		module *mod = iter->second;
		if (strcasecmp(mod->name, name) == 0) {
			*base = iter->first;
			return mod;
		}
	}

    return NULL;
}

/*
 *
 * Add module to a global list. per process's module list only keeps pointers to this global list.
 *
 */
int VMI_add_module(module *mod, const char *key){
	if(mod==NULL)
		return -1;
	string temp(key);
	unordered_map < string, module * >::iterator iter = module_name.find(temp);
	if (iter != module_name.end()){
		return -1;
	}
	module_name[temp]=mod;
	return 1;
}
static SimpleCallback_t VMI_callbacks[VMI_LAST_CB];

DECAF_Handle VMI_register_callback(
                VMI_callback_type_t cb_type,
                VMI_callback_func_t cb_func,
                int *cb_cond
                )
{
  if ((cb_type > VMI_LAST_CB) || (cb_type < 0)) {
    return (DECAF_NULL_HANDLE);
  }

  return (SimpleCallback_register(&VMI_callbacks[cb_type], (SimpleCallback_func_t)cb_func, cb_cond));
}

int VMI_unregister_callback(VMI_callback_type_t cb_type, DECAF_Handle handle)
{
  if ((cb_type > VMI_LAST_CB) || (cb_type < 0)) {
    return (DECAF_NULL_HANDLE);
  }

  return (SimpleCallback_unregister(&VMI_callbacks[cb_type], handle));
}


int VMI_create_process(process *proc)
{
	
	VMI_Callback_Params params;
	params.cp.cr3 = proc->cr3;
	params.cp.pid = proc->pid;
	params.cp.name = proc->name;
    unordered_map < uint32_t, process * >::iterator iter =
    	process_pid_map.find(proc->pid);
    if (iter != process_pid_map.end()){
    	// Found an existing process with the same pid
    	// We force to remove that one.
    //	monitor_printf(default_mon, "remove process pid %d", proc->pid);
    	VMI_remove_process(proc->pid);
    }

    unordered_map < uint32_t, process * >::iterator iter2 =
        	process_map.find(proc->cr3);
    if (iter2 != process_map.end()) {
    	// Found an existing process with the same CR3
    	// We force to remove that process
    //	monitor_printf(default_mon, "removing due to cr3 0x%08x\n", proc->cr3);
    		VMI_remove_process(iter2->second->pid);
    }

   	process_pid_map[proc->pid] = proc;
   	process_map[proc->cr3] = proc;

	SimpleCallback_dispatch(&VMI_callbacks[VMI_CREATEPROC_CB], &params);

#if 0
	if(strlen(name)) { //TEST ONLY!! -Heng
		params.lmm.pid = pid;
		params.lmm.cr3 = cr3;
		params.lmm.name = name;
		SimpleCallback_dispatch(&procmod_callbacks[PROCMOD_LOADMAINMODULE_CB], &params);
	}
#endif
	return 0;
}


int VMI_remove_process(uint32_t pid)
{
	VMI_Callback_Params params;
	process *proc;
    unordered_map < uint32_t, process * >::iterator iter =
    	process_pid_map.find(pid);

    if(iter == process_pid_map.end())
    	return -1;

   // params.rp.proc = iter->second;

    params.rp.cr3 = iter->second->cr3;
    params.rp.pid = iter->second->pid;
    params.rp.name = iter->second->name;
    // printf("removing %d %x %s\n", params.rp.pid, params.rp.cr3, params.rp.name);
	SimpleCallback_dispatch(&VMI_callbacks[VMI_REMOVEPROC_CB], &params);

	process_map.erase(iter->second->cr3);
	process_pid_map.erase(iter);
	delete iter->second;

	return 0;
}



int VMI_insert_module(uint32_t pid, uint32_t base, module *mod)
{
	VMI_Callback_Params params;
	params.lm.pid = pid;
	params.lm.base = base;
	params.lm.name = mod->name;
	params.lm.size = mod->size;
	params.lm.full_name = mod->fullname;
	unordered_map<uint32_t, process *>::iterator iter = process_pid_map.find(
			pid);
	process *proc;

	if (iter == process_pid_map.end()) //pid not found
		return -1;

	proc = iter->second;
    params.lm.cr3 = proc->cr3;

	//Now the pages within the module's memory region are all resolved
	//We also need to removed the previous modules if they happen to sit on the same region
	for (uint32_t vaddr = base; vaddr < base + mod->size; vaddr += 4096) {
		proc->resolved_pages.insert(vaddr >> 12);
		proc->unresolved_pages.erase(vaddr >> 12);
		//TODO: UnloadModule callback
		proc->module_list.erase(vaddr);
	}

	//Now we insert the new module in module_list
	proc->module_list[base] = mod;

	check_unresolved_hooks();

	SimpleCallback_dispatch(&VMI_callbacks[VMI_LOADMODULE_CB], &params);

	return 0;
}

int VMI_remove_module(uint32_t pid, uint32_t base)
{
	VMI_Callback_Params params;
	params.rm.pid = pid;
	params.rm.base = base;
	unordered_map<uint32_t, process *>::iterator iter = process_pid_map.find(
			pid);
	process *proc;

	if (iter == process_pid_map.end()) //pid not found
		return -1;

	proc = iter->second;
	params.rm.cr3 = proc->cr3;

	unordered_map<uint32_t, module *>::iterator m_iter = proc->module_list.find(base);
	if(m_iter == proc->module_list.end())
		return -1;

	module *mod = m_iter->second;

	params.rm.name = mod->name;
	params.rm.size = mod->size;
	params.rm.full_name = mod->fullname;

	for (uint32_t vaddr = base; vaddr < base + mod->size; vaddr += 4096) {
		proc->resolved_pages.erase(vaddr >> 12);
		proc->unresolved_pages.erase(vaddr >> 12);
	}

	proc->module_list.erase(m_iter);

	return 0;
}

int VMI_update_name(uint32_t pid, char *name)
{
	monitor_printf(default_mon,"updating name : not implemented\n");
}
int VMI_remove_all()
{
	monitor_printf(default_mon,"remove all not implemented\n");
}

int VMI_dipatch_lmm(process *proc)
{

	VMI_Callback_Params params;
	params.cp.cr3 = proc->cr3;
	params.cp.pid = proc->pid;
	params.cp.name = proc->name;

	SimpleCallback_dispatch(&VMI_callbacks[VMI_CREATEPROC_CB], &params);

	return 0;
}
int VMI_dispatch_lm(module * m, process *p, gva_t base)
{
  VMI_Callback_Params params;
  params.lm.pid = p->pid;
  params.lm.base = base;
  params.lm.name = m->name;
  params.lm.size = m->size;
  params.lm.full_name = m->fullname;
    params.lm.cr3 = p->cr3;
        SimpleCallback_dispatch(&VMI_callbacks[VMI_LOADMODULE_CB], &params);
}

void VMI_init()
{
#ifdef CONFIG_VMI_ENABLE
	monitor_printf(default_mon, "inside vmi init \n");
	insn_handle_c = DECAF_register_callback(DECAF_TLB_EXEC_CB, block_end_cb, NULL);
#endif

}


