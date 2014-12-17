/*
 * apitracer.c
 *
 *  Created on: Jun 8, 2012
 *      Author: Aravind Prakash (arprakas@syr.edu)
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <glib.h>

#include <sys/time.h>
#include <math.h>


#include "vmi_callback.h"
#include "vmi_c_wrapper.h"
#include "DECAF_main.h"
//#include "DECAF_target.h"
//#include "DECAF_callback.h"

#include "hookapi.h"

#include "helper.h"
#include "parser.h"
#include "handlers.h"

#include "function_map.h"



/* Plugin interface */
static plugin_interface_t apitracer_interface;

struct monitored_proc mon_proc;

DECAF_Handle handle_load_mainmodule = DECAF_NULL_HANDLE;
DECAF_Handle handle_remov_process = DECAF_NULL_HANDLE;

//when monitor all processes,use this path to create new tracefile for new created process.
char *tracefile_path_root[512]={'\0'};

void do_tracing_stop (Monitor *mon, const QDict *qdict)
{
	int i;

	if (mon_proc.pid == 0) {
		monitor_printf(mon, "Not tracing.\n");
		return;
	}

	if(mon_proc.tracefile) {
		fclose(mon_proc.tracefile);
		mon_proc.cr3 = mon_proc.pid = 0;
		mon_proc.tracefile = NULL;
		memset(mon_proc.name, 0, sizeof(mon_proc.name));
	}
}

static void print_ghash_table(GHashTable *addr_mr_map)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, addr_mr_map);
	monitor_printf(default_mon, "Printing hashtable... %d elements\n", g_hash_table_size (addr_mr_map));
	while (g_hash_table_iter_next (&iter, &key, &value))
	  {
		monitor_printf(default_mon, "(0x%08x, 0x%08x), ", (uint32_t)key, (uint32_t)value);
	  }
	monitor_printf(default_mon, "\n");
}

void handle_trace_by_name(const char *procname, const char *config_file, const char *tracefile_path)
{
	int i;
	FILE *tracefile, *configfile;

	if(strcmp(mon_proc.name, procname) == 0) {
		printf("Process %s already being traced.\n", procname);
		return;
	}

	strncpy(mon_proc.name, procname, 512);
	mon_proc.name[511] = '\0';

	tracefile = fopen(tracefile_path, "w");
	if(tracefile == NULL) {
		printf("Unable to open trace file %s for writing. Check filename.\n", tracefile_path);
		goto done;
	}

	configfile = fopen(config_file, "r");
	if(configfile == NULL) {
		printf("Unable to open config file %s. Check filename.\n", config_file);
		goto done;
	}
	fclose(configfile);

	mon_proc.tracefile = tracefile;
	strncpy(mon_proc.configfile, config_file, 512);
	mon_proc.configfile[511] = '\0';


	printf("Waiting for process: %s (case sensitive) to start.\n", mon_proc.name);

done:
	return;

}

void do_trace_by_name(Monitor *mon, const QDict *qdict)
{
	const char *procname = qdict_get_str(qdict, "proc_name");
	const char *config_file = qdict_get_str(qdict, "configfile");
	const char *tracefile_path = qdict_get_str(qdict, "tracefile");

	handle_trace_by_name(procname, config_file, tracefile_path);
}

static mon_cmd_t apitracer_term_cmds[] = {
#include "plugin_cmds.h"
  {NULL, NULL, },
};

void apitracer_cleanup()
{
	int i;

	if(handle_load_mainmodule!=0)
		VMI_unregister_callback(VMI_CREATEPROC_CB,handle_load_mainmodule);
	if(handle_remov_process!=0)
		VMI_unregister_callback(VMI_REMOVEPROC_CB,handle_remov_process);


	if(mon_proc.tracefile){
		fclose(mon_proc.tracefile);
	}

	memset(&mon_proc, 0, sizeof(mon_proc));
}

/* Callback to notify when a program is loaded. */
static void apitracer_loadmainmodule_notify(VMI_Callback_Params *pcp)
{
	char *errorstring;
	int i;
	uint32_t pid=pcp->cp.pid;
	char *name=pcp->cp.name;

	if(strlen(mon_proc.name) > 0) {
		if(strcmp(mon_proc.name, name) == 0) {
			mon_proc.pid = pid;
			mon_proc.cr3 = VMI_find_cr3_by_pid_c(pid);
			if(api_parse(mon_proc.configfile, &errorstring, VMI_find_cr3_by_pid_c(pid)) != 0) {
				printf("%s", errorstring);
				return;
			}
		}
	}
}

static void apitracer_procexit(VMI_Callback_Params *pcp)
{
	int i = 0;
	uint32_t pid=pcp->rp.pid;
	if(pid == mon_proc.pid) {
		fclose(mon_proc.tracefile);
		mon_proc.tracefile = NULL;
	}
}

/* Register the plugin and the callbacks */
plugin_interface_t * init_plugin()
{
  apitracer_interface.mon_cmds = apitracer_term_cmds;
  apitracer_interface.plugin_cleanup = apitracer_cleanup;
  handle_load_mainmodule=VMI_register_callback(VMI_CREATEPROC_CB,apitracer_loadmainmodule_notify,&should_monitor);
  handle_remov_process=VMI_register_callback(VMI_REMOVEPROC_CB,apitracer_procexit,&should_monitor);
  return &apitracer_interface;
}

