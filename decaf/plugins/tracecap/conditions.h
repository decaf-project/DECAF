/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
           Zhenkai Liang <liangzk@comp.nus.edu.sg>
*/
#ifndef _CONDITIONS_H_
#define _CONDITIONS_H_

#include <inttypes.h>
#include "DECAF_main.h" // AWH

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
extern int (*comparestring)(const char *, const char*);
extern int tracing_start_condition;

extern void procname_clear(void);
extern char *procname_get(void);
extern void procname_set(const char *name);
extern int procname_match(const char *name);
extern int procname_is_set(void);

extern void modname_clear(void);
extern void modname_set(const char *name);
extern int modname_match(const char *name);
extern int modname_is_set(void);


extern int uint32_compare(const void* u1, const void* u2);
#if 0 // AWH - Change in cmd interface
extern void tc_modname(const char *modname);
extern void tc_address(uint32_t address);
extern void tc_address_start(uint32_t address, uint32_t at_counter);
extern void tc_address_stop(uint32_t address, uint32_t at_counter);
#else
extern void tc_modname(Monitor *mon, const QDict *qdict);
extern void tc_address(Monitor *mon, const QDict *qdict);
extern void tc_address_start(Monitor *mon, const QDict *qdict);
extern void tc_address_stop(Monitor *mon, const QDict *qdict);
#endif // AWH

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _CONDITIONS_H_

