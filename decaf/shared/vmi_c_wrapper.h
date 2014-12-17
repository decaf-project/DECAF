/*
 * vmi_c_wrapper.h
 *
 *  Created on: Dec 11, 2013
 *      Author: hu
 */

#ifndef VMI_C_WRAPPER_H_
#define VMI_C_WRAPPER_H_

#include "DECAF_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/// a structure of module information
typedef struct _tmodinfo
{
	char	    name[512]; ///< module name
	uint32_t	base;  ///< module base address
	uint32_t	size;  ///< module size
}tmodinfo_t;

typedef struct _old_tmodinfo
{
  char name[32];
  uint32_t base;
  uint32_t size;
}old_modinfo_t;

typedef struct _procinfo
{
  uint32_t pid;
  uint32_t cr3;
  size_t n_mods;
  char name[512];
}procinfo_t;


/// @ingroup semantics
/// locate the module that a given instruction belongs to
/// @param eip virtual address of a given instruction
/// @param cr3 memory space id: physical address of page table
/// @param proc process name (output argument)
/// @param tm return tmodinfo_t structure
extern int   VMI_locate_module_c(gva_t eip, gva_t cr3, char proc[],tmodinfo_t *tm);

//extern int checkcr3(uint32_t cr3, uint32_t eip, uint32_t tracepid, char *name,
  //           int len, uint32_t * offset);

extern int VMI_locate_module_byname_c(const char *name, uint32_t pid,tmodinfo_t * tm);


extern int VMI_find_cr3_by_pid_c(uint32_t pid);

extern int VMI_find_pid_by_cr3_c(uint32_t cr3);

extern int VMI_find_pid_by_name_c(const char* proc_name);

/// @ingroup semantics
/// find process given a memory space id
/// @param cr3 memory space id: physical address of page table
/// @param proc process name (output argument)
/// @param pid  process pid (output argument)
/// @return number of modules in this process
extern int VMI_find_process_by_cr3_c(uint32_t cr3, char proc_name[], size_t len, uint32_t *pid);
/* find process name and CR3 using the PID as search key  */
extern int VMI_find_process_by_pid_c(uint32_t pid, char proc_name[], size_t len, uint32_t *cr3);

extern int VMI_get_proc_modules_c(uint32_t pid, uint32_t mod_no, tmodinfo_t *buf);

extern int VMI_get_all_processes_count_c(void);
/* Create array with info about all processes running in system
    */
extern int VMI_find_all_processes_info_c(size_t num_proc, procinfo_t *arr);


//Aravind - added to get the number of loaded modules for the process. This is needed to create the memory required by get_proc_modules
extern int VMI_get_loaded_modules_count_c(uint32_t pid);
//end - Aravind


/// @ingroup semantics
/// @return the current thread id. If for some reason, this operation
/// is not successful, the return value is set to -1.
/// This function only works in Windows XP for Now.
extern int VMI_get_current_tid_c(CPUState* env);



//0 unknown 1 windows 2 linux
extern int VMI_get_guest_version_c(void);


extern int VMI_list_modules(Monitor *mon, uint32_t pid);
extern int VMI_list_processes(Monitor *mon);

#ifdef __cplusplus
}
#endif
extern target_ulong VMI_guest_kernel_base;

#endif /* VMI_C_WRAPPER_H_ */
