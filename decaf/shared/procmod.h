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
#ifndef _PROCMOD_H_INCLUDED
#define _PROCMOD_H_INCLUDED

#include "monitor.h" // AWH
#include "shared/DECAF_types.h"


#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////
/// @defgroup semantics OS-Aware: OS-level Semantics Extractor
///
/// @ingroup semantics

#define GUEST_MESSAGE_LEN 512
#define GUEST_MESSAGE_LEN_MINUS_ONE 511
/// a structure of module information
typedef struct _tmodinfo
{
	char	    name[512]; ///< module name
	target_ulong	base;  ///< module base address
	target_ulong	size;  ///< module size
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

typedef struct _CreateProc_Params
{
  uint32_t pid;
  uint32_t cr3;
}CreateProc_Params;
typedef void (*createproc_notify_t)(CreateProc_Params* params);

typedef struct _RemoveProc_Params
{
  uint32_t pid;
}RemoveProc_Params;
typedef void (*removeproc_notify_t)(RemoveProc_Params* params);

typedef struct _LoadModule_Params
{
  uint32_t pid;
  uint32_t cr3;
  char* name;
  gva_t base;
  uint32_t size;
  char* full_name;
}LoadModule_Params;
typedef void (*loadmodule_notify_t)(LoadModule_Params* params);

typedef struct _LoadMainModule_Params
{
  uint32_t pid;
  uint32_t cr3;
  const char* name;
}LoadMainModule_Params;
typedef void (*loadmainmodule_notify_t)(LoadMainModule_Params* params);

typedef union _procmod_Callback_Params
{
  CreateProc_Params cp;
  RemoveProc_Params rp;
  LoadModule_Params lm;
  LoadMainModule_Params lmm;
} procmod_Callback_Params;

typedef enum {
  PROCMOD_CREATEPROC_CB = 0,
  PROCMOD_REMOVEPROC_CB,
  PROCMOD_PROCESSEND_CB = PROCMOD_REMOVEPROC_CB, //alias
  PROCMOD_LOADMODULE_CB,
  PROCMOD_LOADMAINMODULE_CB,
  PROCMOD_PROCESSBEGIN_CB = PROCMOD_LOADMAINMODULE_CB, //alias
  PROCMOD_LAST_CB, //place holder for the last position, no other uses.
} procmod_callback_type_t;

typedef void (*procmod_callback_func_t) (procmod_Callback_Params* params);

DECAF_Handle procmod_register_callback(
                procmod_callback_type_t cb_type,
                procmod_callback_func_t cb_func,
                int *cb_cond
                );

int procmod_unregister_callback(procmod_callback_type_t cb_type, DECAF_Handle handle);

/// @ingroup semantics
/// locate the module that a given instruction belongs to
/// @param eip virtual address of a given instruction
/// @param cr3 memory space id: physical address of page table
/// @param proc process name (output argument)
/// @return tmodinfo_t structure 
extern tmodinfo_t * locate_module(uint32_t eip, uint32_t cr3, char proc[]);

/// @ingroup semantics
/// find process given a memory space id
/// @param cr3 memory space id: physical address of page table
/// @param proc process name (output argument)
/// @param pid  process pid (output argument)
/// @return number of modules in this process 
extern int find_process(uint32_t cr3, char proc[], size_t len, uint32_t *pid);

//int delete_thread_by_tid(uint32_t tid);
//int remove_threads_by_cr3(uint32_t cr3);

/// @ingroup semantics
/// @return the current thread id. If for some reason, this operation 
/// is not successful, the return value is set to -1. 
/// This function only works in Windows XP for Now. 
extern uint32_t get_current_tid(CPUState* env);

extern void get_proc_modules(uint32_t pid, old_modinfo_t *buf, int size);


//Aravind - added to get the number of loaded modules for the process. This is needed to create the memory required by get_proc_modules
extern int get_loaded_modules_count(uint32_t pid);
//end - Aravind

extern int procmod_init(void);

extern void procmod_cleanup(void);

extern uint32_t find_cr3(uint32_t pid);

extern uint32_t find_pid(uint32_t cr3);

extern uint32_t find_pid_by_name(const char* proc_name);

extern void list_procs(Monitor *mon); // AWH void);
//extern void do_linux_ps(Monitor *mon); // AWH void);
extern void linux_ps(Monitor *mon, int mmap_flag);
extern void list_guest_modules(Monitor *mon, uint32_t pid);

void parse_process(const char *log);
void parse_module(const char *log);

/// @ingroup semantics
/// This function is only used to update process and module information for Linux
extern void update_proc(void *opaque);

extern int checkcr3(uint32_t cr3, uint32_t eip, uint32_t tracepid, char *name,
             int len, uint32_t * offset);

/// @ingroup semantics
/// This function inserts the module information 
extern int procmod_insert_modinfo(uint32_t pid, uint32_t cr3, const char *name,
                           uint32_t base, uint32_t size);


extern tmodinfo_t *locate_module_byname(const char *name, uint32_t pid);

/* Create array with info about all processes running in system 
   Caller is in charge of freeing the returned array */
extern procinfo_t* find_all_processes_info(size_t *num_proc);

/* find process name and CR3 using the PID as search key  */
extern int find_process_by_pid(uint32_t pid, char proc_name[], size_t len, uint32_t *cr3);

extern int is_guest_windows(void);

void handle_guest_message(const char *message);

#ifdef __cplusplus 
};
#endif

#endif

