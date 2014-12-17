/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
           Zhenkai Liang <liangzk@comp.nus.edu.sg>
*/
#ifndef _TRACECAP_H_
#define _TRACECAP_H_

#include <stdio.h>
#include <inttypes.h>
#include <sys/user.h>
#include "trace.h"
#include "../shared/hookapi.h"



#undef INLINE
#define TAINT_LOOP_IVS

/* Some configuration options that we don't foresee people to change 
 * Thus, they are not part of the ini configuration file */
#define PRINT_FUNCTION_MAP 1


/* Exit codes */
#define EXIT_ERROR -1
#define EXIT_NORMAL 1
#define EXIT_KILL_SIGNAL 13
#define EXIT_KILL_MSG 13
#define EXIT_DETECT_TAINTEIP 21
#define EXIT_DETECT_EXCEPTION 22
#define EXIT_DETECT_NULLPTR 23
#define EXIT_DETECT_PROCESSEXIT 24

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* External Variables */
extern FILE *tracelog;
extern FILE *tracenetlog;
extern FILE *tracehooklog;
extern FILE *calllog;
extern FILE *alloclog;
extern uint32_t tracepid;
extern uint32_t tracecr3;
extern EntryHeader eh;

extern uint32_t current_tid; // Current thread id
extern int skip_decode_address; // If != 0, decode_address will not be called
extern int skip_trace_write; // If != 0, write_insn will not be called

extern int skip_decode_address; // If != 0, decode_address will be called
extern void (*hook_insn_begin) (uint32_t eip);
extern char *tracename_p;
extern unsigned int tracing_child; // If !=0, we are tracing child
extern uint32_t insn_tainted;
#if 0 // AWH - new monitor cmd interface
/* Functions */
void do_taint_sendkey(const char *key, int taint_origin, int taint_offset);
// AWH - Added "type" parm for new blockdev interface
void do_taint_file(char *filename, int dev_index, uint32_t taint_id, BlockInterfaceType type);
void do_linux_ps_opt(int has_mmap_flag, int mmap_flag);
void do_tracing(uint32_t pid, const char *filename);
void do_tracing_by_name(const char *progname, const char *filename);
void do_save_state(uint32_t pid, uint32_t address, const char *filename);
void do_guest_modules(uint32_t pid);
void do_add_iv_eip(uint32_t eip);
void do_clean_iv_eips(void);
#else
extern void do_taint_sendkey(Monitor *mon, const QDict *qdict);
// AWH - No Sleuthkit for now
//extern void do_taint_file(Monitor *mon, const QDict *qdict);
extern void do_linux_ps_opt(Monitor *mon, const QDict *qdict);
extern void do_tracing(Monitor *mon, const QDict *qdict);
extern void do_tracing_by_name(Monitor *mon, const QDict *qdict);
extern void do_save_state(Monitor *mon, const QDict *qdict);
extern void do_guest_modules(Monitor *mon, const QDict *qdict);
extern void do_add_iv_eip(Monitor *mon, const QDict *qdict);
extern void do_clean_iv_eips(Monitor *mon);
extern void do_tracing_stop(Monitor *mon);
extern void do_load_hooks(Monitor *mon, const QDict *qdict);
#endif // AWH

int tracing_start(uint32_t pid, const char *filename);
void tracing_stop(void);
void taint_loop_ivs(void);

#if TRACECAP_REGRESSION
void my_cleanup(uint32_t pid);
#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _TRACECAP_H_
