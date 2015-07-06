/*
 * vmi_c_wraper.c
 *
 *  Created on: Dec 11, 2013
 *      Author: hu
 */

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
//#include "windows_vmi.h"
//#include "linux_vmi.h"

#include "hookapi.h"
// AWH #include "read_linux.h"
#include "shared/vmi.h"
#include "shared/DECAF_main.h"
#include "shared/hookapi.h"
#include "shared/function_map.h"
#include "shared/utils/SimpleCallback.h"
#include "shared/utils/Output.h"

#include "linux_vmi_new.h"
#include "vmi_c_wrapper.h"


using namespace std;
using namespace std::tr1;

//#ifdef CONFIG_VMI_ENABLE
//keep the sized same with that defined in vmi.h
#define MODULE_NAME_SIZE 32
#define MODULE_FULLNAME_SIZE 256
#define PROCESS_NAME_SIZE 16
int  VMI_locate_module_c(gva_t eip, gva_t cr3, char proc[],tmodinfo_t *tm)
{
	module *m;
	process *p;
    gva_t base = 0;

    p = VMI_find_process_by_pgd(cr3);
    if (!p)
    	return -1;

	m = VMI_find_module_by_pc(eip, cr3, &base);
	if(!m)
		return -1;
	strncpy(tm->name, m->name, MODULE_NAME_SIZE ) ;
	strncpy(proc, p->name, sizeof(p->name));
	tm->base = base;
	tm->size = m->size;
	return 0;

}

int VMI_locate_module_byname_c(const char *name, uint32_t pid,tmodinfo_t * tm)
{
	module * m = NULL;
	process * p = NULL;
	gva_t base = 0;

	if (!tm) {
		DECAF_printf("tm is NULL\n");
		return -1;
	}
    p = VMI_find_process_by_pid(pid);
    if(!p)
    	return -1;
    m = VMI_find_module_by_name(name, p->cr3,&base);
    if(!m)
    	return -1;
	strncpy(tm->name,m->name, MODULE_NAME_SIZE ) ;
	tm->base = base;
	tm->size = m->size ;
	return 0;

}


int VMI_find_cr3_by_pid_c(uint32_t pid)
{
	process * p = NULL;
	p = VMI_find_process_by_pid(pid);
	if(!p)
		return -1;
	return p->cr3;

}

int VMI_find_pid_by_cr3_c(uint32_t cr3)
{
	process * p = NULL;
	p  = VMI_find_process_by_pgd(cr3);
	if(!p)
		return -1;
	return p->pid;
}

int VMI_find_pid_by_name_c(const char* proc_name)
{
	process *p = NULL;
	p = VMI_find_process_by_name(proc_name);
	if(!p)
		return -1;
	return p->pid;
}


int VMI_find_process_by_cr3_c(uint32_t cr3, char proc_name[], size_t len, uint32_t *pid)
{
	process *p = NULL;
	p = VMI_find_process_by_pgd(cr3);
	if(!p)
		return -1;
	if(len > PROCESS_NAME_SIZE)
		strncpy(proc_name,p->name,PROCESS_NAME_SIZE);
	else
		strncpy(proc_name,p->name,len);
	*pid = p->pid;

	return 0;

}

int VMI_find_process_by_pid_c(uint32_t pid, char proc_name[], size_t len, uint32_t *cr3)
{
	process *p = NULL;
	p = VMI_find_process_by_pid(pid);
	if(!p)
		return -1;
	if(len > PROCESS_NAME_SIZE)
		strncpy(proc_name,p->name,PROCESS_NAME_SIZE);
	else
		strncpy(proc_name,p->name,len);
	*cr3 = p->cr3;

	return 0;

}
int VMI_get_loaded_modules_count_c(uint32_t pid)
{
	process *p = NULL;
	p = VMI_find_process_by_pid(pid);
	if(!p)
		return -1;
	return p->module_list.size();

}
int VMI_get_proc_modules_c(uint32_t pid, uint32_t mod_no, tmodinfo_t *buf)
{
	   process *p = NULL;
	   p = VMI_find_process_by_pid(pid);
	   module * m = NULL;
	   if(!p)
		   return -1;
		unordered_map<uint32_t, module *>::iterator iter;
		uint32_t index = 0;
		for (iter = p->module_list.begin(); iter != p->module_list.end();
				iter++) {
			 m = iter->second;
			 buf[index].size = m->size;
			 buf[index].base = iter->first;
			 strncpy(buf[index].name, m->name, MODULE_NAME_SIZE);
			 index ++;
		}
		return 0;

}
int VMI_get_all_processes_count_c(void)
{
	return process_map.size();
}

int VMI_find_all_processes_info_c(size_t num_proc, procinfo_t *arr)
{
	    unordered_map < uint32_t, process * >::iterator iter;
	    size_t nproc;
	    uint32_t idx = 0;
	    nproc = process_map.size();
	    if(num_proc != nproc)
	    {
	    	DECAF_printf("num_proc is not the same with current process number\n");
	    	return -1;
	    }
	    if(arr){
	    	for (iter = process_map.begin(); iter != process_map.end(); iter++) {
	    		process * proc = iter->second;
	    		arr[idx].cr3 = proc->cr3;
	    		arr[idx].pid = proc->pid;
	    		arr[idx].n_mods = proc->module_list.size();
	    		strncpy(arr[idx].name, proc->name,PROCESS_NAME_SIZE);
	    		arr[idx].name[511] = '\0';
	    		idx++;
	    	}
	    }
	    return 0;

}

int VMI_list_processes(Monitor *mon)
{
	process *proc;
	unordered_map<uint32_t, process *>::iterator iter;

	for (iter = process_map.begin(); iter != process_map.end(); iter++) {
		proc = iter->second;
		monitor_printf(mon, "%d\tcr3=0x%08x\t%s\n", proc->pid, proc->cr3,
				proc->name);
	}

	return 0;
}


int VMI_list_modules(Monitor *mon, uint32_t pid) {
	unordered_map<uint32_t, process *>::iterator iter = process_pid_map.find(
			pid);
	if (iter == process_pid_map.end())
	{
		monitor_printf(default_mon,"pid not found\n");
		//pid not found
		return -1;
	}

	unordered_map<uint32_t, module *>::iterator iter2;
	process *proc = iter->second;

	if(!proc->modules_extracted)
		traverse_mmap(cpu_single_env, proc);

	map<uint32_t, module *> modules;
	map<uint32_t, module *>::iterator iter_m;

	for (iter2 = proc->module_list.begin(); iter2 != proc->module_list.end();
			iter2++) {
		modules[iter2->first] = iter2->second;
	}

	for (iter_m = modules.begin(); iter_m!=modules.end(); iter_m++) {
		module *mod = iter_m->second;
		uint32_t base = iter_m->first;
		monitor_printf(mon, "%20s\t0x%08x\t0x%08x\n", mod->name, base,
			mod->size);
	}


	return 0;
}


int VMI_get_guest_version_c(void)
{
	if(VMI_guest_kernel_base ==0x80000000)
		return 1;//windows
	else if(VMI_guest_kernel_base == 0xC0000000)
		return 2;//linux
	return 0;//unknown
}
int VMI_get_current_tid_c(CPUState* _env)
{
#ifdef TARGET_I386
	uint32_t val;
	uint32_t tid;

	//This may only work with Windows XP

//	if (!is_guest_windows())
	//	return -1;
	if (VMI_guest_kernel_base != 0x80000000)
		return -1;

	if (!DECAF_is_in_kernel(_env)) { // user module
		if (DECAF_read_mem(_env, /* AWH cpu_single*/_env->segs[R_FS].base + 0x18, 4, &val) != -1
				&& DECAF_read_mem(_env, val + 0x24, 4, &tid) != -1)
			return tid;
	} else if (DECAF_read_mem(_env, /* AWH cpu_single*/_env->segs[R_FS].base + 0x124, 4, &val)
			!= -1 && DECAF_read_mem(_env, val + 0x1F0, 4, &tid) != -1)
		return tid;
#endif

	return -1;
}

//#endif /*CONFIG_VMI_ENABLE*/
