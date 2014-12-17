/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
           Zhenkai Liang <liangzk@comp.nus.edu.sg>
*/
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include "conditions.h"
#include "DECAF_main.h"
#include "DECAF_target.h"
#include "hookapi.h"
#include "conf.h"

int (*comparestring)(const char *, const char*);

/* ==== start and stop conditions ==== */

int tracing_start_condition = 1;

/* trace by name */
static char tracename[64];

void procname_clear() 
{
    tracename[0] = 0; 
}

char *procname_get() 
{
    return tracename; 
}

void procname_set(const char *name)
{
    strncpy(tracename, name, sizeof(tracename));
}

int procname_match(const char *name)
{
    int retval = 0; 
    if(tracename[0]==0)
    	return retval;
    if (strcmp(tracename, name) == 0)
	retval = 1;

    return retval; 
}

int procname_is_set()
{
    return (tracename[0] != 0);
}


/* start tracing on entering module */
static char cond_modulename[64];

void modname_clear() 
{
    cond_modulename[0] = 0; 
}

void modname_set(const char *name) 
{
    strncpy(cond_modulename, name, sizeof(cond_modulename)); 
}

int modname_match(const char *name)
{
    int retval = 0; 

    if (comparestring(cond_modulename, name) == 0)
	retval = 1; 

    return retval;
}

int modname_is_set()
{
    return (cond_modulename[0] != 0);
}

/* Start tracing on different conditions */
static uint32_t tc_start_counter = 0;
static uint32_t tc_start_at = 0;
static uint32_t tc_stop_counter = 0;
static uint32_t tc_stop_at = 0;
static uint32_t tc_stop_address = 0;
static uint32_t tc_stop_hook_handle = 0;
static uint32_t cond_func_address;
static uint32_t cond_func_hook_handle = 0;


//void tc_modname(const char *modname)
void tc_modname(Monitor *mon, const QDict *qdict)
{
    strncpy(cond_modulename, /*modname*/ qdict_get_str(qdict, "tc_modname"), 64);
    tracing_start_condition = 0;
}

/* AWH int */ void tc_address_hook(void *opaque)
{
  if (decaf_plugin->monitored_cr3 == cpu_single_env->cr[3]) {
    tracing_start_condition = 1;
    /* remove the hook */
    hookapi_remove_hook(cond_func_hook_handle);
   }

   // AWH return 0;
}

//void tc_address(uint32_t address)
void tc_address(Monitor *mon, const QDict *qdict)
{
  int address = qdict_get_int(qdict, "codeaddress");

  /* Check if there is a conflict with conf_trace_only_after_first_taint */
  if (conf_trace_only_after_first_taint) {
    monitor_printf(default_mon, "tc_address_start conflicts with "
      "conf_trace_only_after_first_taint\n"
      "Disabling conf_trace_only_after_first_taint\n");
    conf_trace_only_after_first_taint = 0;
  }
  /* add a hook at address */
  tracing_start_condition = 0;
  cond_func_hook_handle = hookapi_hook_function(0, address, 0, tc_address_hook,
		NULL, 0);
  cond_func_address = address;
}

/* AWH int */ void tc_address_start_hook(void *opaque)
{
  monitor_printf(default_mon, "tc_address_start_hook(*) called\n");
  if ((tracing_kernel_all() ||
    (decaf_plugin->monitored_cr3 == cpu_single_env->cr[3])) &&
    (tc_start_counter++ == tc_start_at))
  {
    tracing_start_condition = 1;
    tc_stop_counter = 0; // reset the tc_stop_counter at the execution saving
    /* remove the hook */
    hookapi_remove_hook(cond_func_hook_handle);
  }

  // AWH return 0;
}

//void tc_address_start(uint32_t address, uint32_t at_counter)
void tc_address_start(Monitor *mon, const QDict *qdict)
{
  uint32_t address, at_counter;
  address = qdict_get_int(qdict, "codeaddress");
  at_counter = qdict_get_int(qdict, "timehit");

  /* Check if there is a conflict with conf_trace_only_after_first_taint */
  if (conf_trace_only_after_first_taint) {
    monitor_printf(mon, "tc_address_start conflicts with "
      "conf_trace_only_after_first_taint\n"
      "Disabling conf_trace_only_after_first_taint\n");
    conf_trace_only_after_first_taint = 0;
  }
  /* add a hook at address */
  tracing_start_condition = 0;
  tc_start_counter = 0;
  tc_start_at = at_counter;
  cond_func_hook_handle = hookapi_hook_function(0, address, 0,
	tc_address_start_hook, NULL, 0);
  cond_func_address = address;
}

/* AWH int */ void tc_address_stop_hook(void *opaque)
{
  monitor_printf(default_mon, "tc_address_stop_hook(*) called\n");
  if ((tracing_kernel_all() ||
    (decaf_plugin->monitored_cr3 == cpu_single_env->cr[3])) &&
    (tc_stop_counter++ == tc_stop_at))
  {
    tracing_start_condition = 0;
    /* Properly call tracing_stop to stop tracing and
       perform other related operations (such as saving
       the state file) */
    tracing_stop();
    /* remove the hook */
    hookapi_remove_hook(tc_stop_hook_handle);
  }

  // AWH return 0;
}

//void tc_address_stop(uint32_t address, uint32_t at_counter)
void tc_address_stop(Monitor *mon, const QDict *qdict) 
{
  uint32_t address, at_counter;
  address = qdict_get_int(qdict, "codeaddress");
  at_counter = qdict_get_int(qdict, "timehit");

  /* add a hook at address */
  tc_stop_counter = 0;
  tc_stop_at = at_counter;
  tc_stop_hook_handle = hookapi_hook_function(0, address, 0,
		tc_address_stop_hook, NULL, 0);
  tc_stop_address = address;
}


