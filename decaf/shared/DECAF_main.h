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
 * DECAF_main.h
 *
 *  Created on: Oct 14, 2012
 *      Author: lok
 *  This is half of the old main.h. All of the declarations here are
 *  target independent. All of the target dependent declarations and code
 *  are in the target directory in DECAF_main_x86.h and .c for example
 */


#include "qemu-common.h"
#include "monitor.h"
#include "DECAF_types.h"
#include "blockdev.h"

#ifndef DECAF_MAIN_H_
#define DECAF_MAIN_H_


#ifdef __cplusplus
extern "C"
{
#endif





#define PAGE_LEVEL 0
#define BLOCK_LEVEL 1
#define ALL_CACHE 2




/*************************************************************************
 * The Plugin interface comes first
 *************************************************************************/


/// primary structure for DECAF plugin,
// callbacks have been removed due to the new interface
// including callbacks and states
// tainting has also been removed since we are going to
// have a new tainting interface that is dynamically
// controllable - which will be more like a util than
// something that is built directly into DECAF
typedef struct _plugin_interface {
  /// array of monitor commands
  const mon_cmd_t *mon_cmds; // AWH - was term_cmd_t *term_cmds
  /// array of informational commands
  const mon_cmd_t *info_cmds; // AWH - was term_cmd_t
  /*!
   * \brief callback for cleaning up states in plugin.
   * TEMU plugin must release all allocated resources in this function
   */
  void (*plugin_cleanup)(void);

  //TODO: may need to remove it.
  //void (*send_keystroke) (int reg);

  //TODO: need to change it into using our generic callback interface
  void (*after_loadvm) (const char *param);

  /// \brief CR3 of a specified process to be monitored.
  /// 0 means system-wide monitoring, including all processes and kernel.
  union
  {
    uint32_t monitored_cr3;
    uint32_t monitored_pgd; //alias
  };
} plugin_interface_t;

extern plugin_interface_t *decaf_plugin;

void do_load_plugin_internal(Monitor *mon, const char *plugin_path);
int do_load_plugin(Monitor *mon, const QDict *qdict, QObject **ret_data);
int do_unload_plugin(Monitor *mon, const QDict *qdict, QObject **ret_data);

/*************************************************************************
 * The Virtual Machine control
 *************************************************************************/

/// Pause the guest system
void DECAF_stop_vm(void);
// Unpause the guest system
void DECAF_start_vm(void);


/*************************************************************************
 * Functions for accessing the guest's memory
 *************************************************************************/

/****** Functions used by DECAF plugins ****/
extern void DECAF_physical_memory_rw(CPUState* env, gpa_t addr, uint8_t *buf,
                            int len, int is_write);

#define DECAF_physical_memory_read(_env, addr, buf, len) \
        DECAF_physical_memory_rw(_env, addr, buf, len, 0)

#define DECAF_physical_memory_write(_env, addr, buf, len) \
        DECAF_physical_memory_rw(_env, addr, buf, len, 1)

/// Convert virtual address into physical address
extern gpa_t DECAF_get_phys_addr(CPUState* env, gva_t addr);

/// Convert virtual address into physical address for given cr3 - cr3 is a phys addr
//The implementation is target-specific
//extern gpa_t DECAF_get_physaddr_with_cr3(CPUState* env, target_ulong cr3, gva_t addr);

extern gpa_t DECAF_get_phys_addr_with_pgd(CPUState* env, gpa_t pgd, gva_t addr);

//wrapper -- pgd is the generic term while cr3 is the register in x86
#define  DECAF_get_physaddr_with_cr3(_env, _pgd, _addr) DECAF_get_phys_addr_with_pgd(_env, _pgd, _addr)

extern DECAF_errno_t DECAF_memory_rw(CPUState* env, gva_t addr, void *buf, int len, int is_write);

DECAF_errno_t DECAF_memory_rw_with_pgd(CPUState* env, target_ulong pgd, gva_t addr, void *buf,
                            int len, int is_write);

/// \brief Read from a memory region by its virtual address.
///
/// @param vaddr virtual memory address
/// @param len length of memory region (in bytes)
/// @param buf output buffer of the value to be read
/// @return status: 0 for success and -1 for failure
///
/// If failure, it usually means that the given virtual address cannot be converted
/// into physical address. It could be either invalid address or swapped out.
extern DECAF_errno_t DECAF_read_mem(CPUState* env, gva_t vaddr, int len, void *buf);

/// \brief Write into a memory region by its virtual address.
///
/// @param vaddr virtual memory address
/// @param len length of memory region (in bytes)
/// @param buf input buffer of the value to be written
/// @return status: 0 for success and -1 for failure
///
/// If failure, it usually means that the given virtual address cannot be converted
/// into physical address. It could be either invalid address or swapped out.
extern DECAF_errno_t DECAF_write_mem(CPUState* env, gva_t vaddr, int len, void *buf);


extern DECAF_errno_t DECAF_read_mem_with_pgd(CPUState* env, target_ulong pgd, gva_t vaddr, int len, void *buf);
extern DECAF_errno_t DECAF_write_mem_with_pgd(CPUState* env, target_ulong pgd, gva_t vaddr, int len, void *buf);
DECAF_errno_t DECAF_read_ptr(CPUState *env, gva_t vaddr, gva_t *pptr);


extern void * DECAF_KbdState;
extern void DECAF_keystroke_read(uint8_t taint_status);
extern void DECAF_keystroke_place(int keycode);

/// \brief Set monitor context.
///
/// This is a boolean flag that indicates if the current execution needs to be monitored
/// and analyzed by the plugin. The default value is 1, which means that the plugin wants
/// to monitor all execution (including the OS kernel and all running applications).
/// Very often, the plugin is only interested in a single user-level process.
/// In this case, the plugin is responsible to set this flag to 1 when the execution is within
/// the specified process and to 0 when it is not.
extern int should_monitor;
extern int g_bNeedFlush;

//extern int do_enable_emulation(Monitor *mon, const QDict *qdict, QObject **ret_data);
//extern int do_disable_emulation(Monitor *mon, const QDict *qdict, QObject **ret_data);

// For sleuthkit to read
int DECAF_bdrv_pread(void *opaque, int64_t offset, void *buf, int count);


extern int DECAF_emulation_started; //will be removed

// In DECAF - we do not use the same-per vcpu flushing behavior as in QEMU. For example
// DECAF_flushTranslationCache is a wrapper for tb_flush that iterates through all of
// the virtual CPUs and calls tb_flush on that particular environment. The main reasoning
// behind this decision is that the user wants to know when an event occurs for any
// vcpu and not only for specific ones. This idea can change in the future of course.
// We have yet to decide how to handle multi-core analysis, at the program abstraction
// level or at the thread execution level or at the virtual cpu core level?
// No matter what the decision, flushing can occur using the CPUState as in QEMU
// or using DECAF's wrappers.

/**
 * Flush - or invalidate - the translation block for address addr in the env context.
 * @param env The cpu context
 * @param addr The block's address
 */
extern void DECAF_flushTranslationBlock_env(CPUState* env, gva_t addr);

void DECAF_perform_flush(CPUState* env);

/**
 * Flush - or invalidate - all translation blocks for the page in addr.
 * Note that in most cases TARGET_PAGE_SIZE is 4k in size, which is expected.
 *  However, in some cases it might only be 1k (in ARM). We use TARGET_PAGE_SIZE
 *  as the mask in this function
 *
 * @param env The cpu context
 * @param addr The page address
 */
void DECAF_flushTranslationPage_env(CPUState* env, gva_t addr);

//These are DECAF wrappers that does flushing for all VCPUs

//Iterates through all virtual cpus and flushes the blocks
static inline void DECAF_flushTranslationBlock(uint32_t addr)
{
  CPUState* env;
  for(env = first_cpu; env != NULL; env = env->next_cpu)
  {
    DECAF_flushTranslationBlock_env(env, addr);
  }
}

//Iterates through all virtual cpus and flushes the pages
static inline void DECAF_flushTranslationPage(uint32_t addr)
{
  CPUState* env;
  for(env = first_cpu; env != NULL; env = env->next_cpu)
  {
    DECAF_flushTranslationPage_env(env, addr);
  }
}

//Iterates through all virtual cpus and flushes the pages
void DECAF_flushTranslationCache(int type,target_ulong addr);

/* Static in monitor.c for QEMU, but we use it for plugins: */
///send a keystroke into the guest system
extern void do_send_key(const char *string);




#ifdef __cplusplus
}
#endif

#endif /* DECAF_MAIN_H_ */

