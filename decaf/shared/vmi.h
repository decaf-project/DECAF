/*
 * vmi.h
 *
 *  Created on: Jan 22, 2013
 *      Author: Heng Yin
 */



#ifndef VMI_H_
#define VMI_H_

#include <iostream>
#include <list>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include "vmi_callback.h"
#include "monitor.h"

//#ifdef CONFIG_VMI_ENABLE
using namespace std;
using namespace std::tr1;

//#include "qemu-timer.h"
// #define NAMESIZEC 16
// #define MAX_NAME_LENGTHC 64

#define VMI_MAX_MODULE_PROCESS_NAME_LEN 64
#define VMI_MAX_MODULE_FULL_NAME_LEN 256

class module{
public:
	char name[VMI_MAX_MODULE_PROCESS_NAME_LEN];
	char fullname[VMI_MAX_MODULE_FULL_NAME_LEN];
	uint32_t size;
	uint32_t codesize; // use these to identify dll
	uint32_t checksum;
	uint16_t major;
	uint16_t minor;
	bool	symbols_extracted;
	unordered_map < uint32_t, string> function_map_offset;
	unordered_map < string, uint32_t> function_map_name;
	unsigned int inode_number;

	module()
	{
		this->inode_number = 0;
	}
};


class process{
public:
    uint32_t cr3;
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t EPROC_base_addr;
    char name[VMI_MAX_MODULE_PROCESS_NAME_LEN];
    bool modules_extracted;
    //map base address to module pointer
    unordered_map < uint32_t,module * >module_list;
    //a set of virtual pages that have been resolved with module information
    unordered_set< uint32_t > resolved_pages;
    unordered_map< uint32_t, int > unresolved_pages;
};



typedef enum {
	WINXP_SP2_C = 0, WINXP_SP3_C, WIN7_SP0_C, WIN7_SP1_C, LINUX_GENERIC_C,
} GUEST_OS_C;


typedef struct os_handle_c{
	GUEST_OS_C os_info;
	int (*find)(CPUState *env,uintptr_t insn_handle);
	void (*init)();
} os_handle_c;

extern target_ulong VMI_guest_kernel_base;

extern unordered_map < uint32_t, process * >process_map;
extern unordered_map < uint32_t, process * >process_pid_map;
extern unordered_map < string, module * >module_name;

module * VMI_find_module_by_pc(target_ulong pc, target_ulong pgd, target_ulong *base);

module * VMI_find_module_by_name(const char *name, target_ulong pgd, target_ulong *base);

module * VMI_find_module_by_base(target_ulong pgd, uint32_t base);

process * VMI_find_process_by_pid(uint32_t pid);

process * VMI_find_process_by_pgd(uint32_t pgd);

process* VMI_find_process_by_name(const char *name);

// add one module
int VMI_add_module(module *mod, const char *key);
// find module by key
module* VMI_find_module_by_key(const char *key);

//AVB
int VMI_extract_symbols(module *mod, target_ulong base);

int VMI_create_process(process *proc);
int VMI_remove_process(uint32_t pid);
int VMI_update_name(uint32_t pid, char *name);
int VMI_remove_all();
int VMI_insert_module(uint32_t pid, uint32_t base, module *mod);
int VMI_remove_module(uint32_t pid, uint32_t base);
int VMI_dipatch_lmm(process *proc);
int VMI_dispatch_lm(module * m,process *proc, gva_t base);
int VMI_is_MoudleExtract_Required();

extern "C" void VMI_init();
extern "C" int procmod_init();
extern "C" void handle_guest_message(const char *message);
#endif /* VMI_H_ */

//#endif /*CONFIG_VMI_ENABLE*/
