/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>
This is a plugin of DECAF. You can redistribute and modify it
under the terms of BSD license but it is made available
WITHOUT ANY WARRANTY. See the top-level COPYING file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
/**
 * @author Xunchao Hu, Heng Yin
 * @date Jan 24 2013
 */

#include <sys/time.h>

#include "DECAF_types.h"
#include "DECAF_main.h"
#include "hookapi.h"
#include "DECAF_callback.h"
#include "shared/vmi_callback.h"
#include "utils/Output.h"
#include "custom_handlers.h"

//basic stub for plugins
static plugin_interface_t hookapitests_interface;
static DECAF_Handle processbegin_handle = DECAF_NULL_HANDLE;
static DECAF_Handle removeproc_handle = DECAF_NULL_HANDLE;
static DECAF_Handle blockbegin_handle = DECAF_NULL_HANDLE;
static DECAF_Handle ntcreatefile_handle = DECAF_NULL_HANDLE;
static DECAF_Handle VirtualAlloc_handle = DECAF_NULL_HANDLE;

static char targetname[512];
static uint32_t targetpid = -1;
static uint32_t targetcr3 = 0;

static void hook_all(DECAF_Callback_Params* param) {
	if (targetcr3 != cpu_single_env->cr[3])
		return;
	char modname[512];
	char functionname[512];
	if (0
			== funcmap_get_name_c(param->bb.tb->pc, targetcr3, &modname,
					&functionname)) {
		// print all the apis called by traced program
		printf("%s %s  %s\n", targetname, modname, functionname);
	}

}


typedef struct {
	uint32_t call_stack[12]; //paramters and return address
	DECAF_Handle hook_handle;
} NtCreateFile_hook_context_t;

static void NtCreateFile_ret(void *param)
{
	NtCreateFile_hook_context_t *ctx = (NtCreateFile_hook_context_t *)param;
	DECAF_printf("NtCreateFile exit:");

	hookapi_remove_hook(ctx->hook_handle);
	uint32_t out_handle;

	DECAF_read_mem(NULL, ctx->call_stack[1], 4, &out_handle);
	DECAF_printf("out_handle=%08x\n", out_handle);
	free(ctx);
}

static void NtCreateFile_call(void *opaque)
{
	DECAF_printf("NtCreateFile entry\n");
	NtCreateFile_hook_context_t *ctx = (NtCreateFile_hook_context_t*)
			malloc(sizeof(NtCreateFile_hook_context_t));
	if(!ctx) //run out of memory
		return;

	DECAF_read_mem(NULL, cpu_single_env->regs[R_ESP], 12*4, ctx->call_stack);
	ctx->hook_handle = hookapi_hook_return(ctx->call_stack[0],
			NtCreateFile_ret, ctx, sizeof(*ctx));
}

static void VirtualAlloc_ret(void *param)
{
	NtCreateFile_hook_context_t *ctx = (NtCreateFile_hook_context_t *)param;
	DECAF_printf("VirtualAlloc exit:");

	hookapi_remove_hook(ctx->hook_handle);
	DECAF_printf("lpAddress=%08x, dwSize=%d, ret=%08x\n", ctx->call_stack[1], 
		ctx->call_stack[2], cpu_single_env->regs[R_EAX]);

	free(ctx);

}

static void VirtualAlloc_call(void *opaque)
{
	DECAF_printf("VirtualAlloc entry\n");
	NtCreateFile_hook_context_t *ctx = (NtCreateFile_hook_context_t*)
			malloc(sizeof(NtCreateFile_hook_context_t));
	if(!ctx) //run out of memory
		return;

	DECAF_read_mem(NULL, cpu_single_env->regs[R_ESP], 12*4, ctx->call_stack);
	ctx->hook_handle = hookapi_hook_return(ctx->call_stack[0],
			VirtualAlloc_ret, ctx, sizeof(*ctx));
}

static void register_hooks()
{
	ntcreatefile_handle = hookapi_hook_function_byname(
			"ntdll.dll", "NtCreateFile", 1, targetcr3,
			NtCreateFile_call, NULL, 0);

	VirtualAlloc_handle = hookapi_hook_function_byname(
			"kernel32.dll", "VirtualAlloc", 1, targetcr3,
			VirtualAlloc_call, NULL, 0);

}


static void createproc_callback(VMI_Callback_Params* params)
{
    if(targetcr3 != 0) //if we have found the process, return immediately
    	return;

	if (strcasecmp(targetname, params->cp.name) == 0) {
		targetpid = params->cp.pid;
		targetcr3 = params->cp.cr3;
		register_hooks();
		DECAF_printf("Process found: pid=%d, cr3=%08x\n", targetpid, targetcr3);
	}
}


static void removeproc_callback(VMI_Callback_Params* params)
{
	//Stop the test when the monitored process terminates

}


static void do_hookapitests(Monitor* mon, const QDict* qdict)
{
	if ((qdict != NULL) && (qdict_haskey(qdict, "procname"))) {
		strncpy(targetname, qdict_get_str(qdict, "procname"), 512);
	}
	targetname[511] = '\0';
}


static int hookapitests_init(void)
{
	DECAF_output_init(NULL);
	DECAF_printf("Hello World\n");
	//register for process create and process remove events
	processbegin_handle = VMI_register_callback(VMI_CREATEPROC_CB,
			&createproc_callback, NULL);
	removeproc_handle = VMI_register_callback(VMI_REMOVEPROC_CB,
			&removeproc_callback, NULL);
	if ((processbegin_handle == DECAF_NULL_HANDLE)
			|| (removeproc_handle == DECAF_NULL_HANDLE)) {
		DECAF_printf(
				"Could not register for the create or remove proc events\n");
	}
	return (0);
}

static void hookapitests_cleanup(void)
{
	// procmod_Callback_Params params;

	DECAF_printf("Bye world\n");

	if (processbegin_handle != DECAF_NULL_HANDLE) {
		VMI_unregister_callback(VMI_CREATEPROC_CB,
				processbegin_handle);
		processbegin_handle = DECAF_NULL_HANDLE;
	}

	if (removeproc_handle != DECAF_NULL_HANDLE) {
		VMI_unregister_callback(VMI_REMOVEPROC_CB, removeproc_handle);
		removeproc_handle = DECAF_NULL_HANDLE;
	}
	if (blockbegin_handle != DECAF_NULL_HANDLE) {
		DECAF_unregister_callback(DECAF_BLOCK_BEGIN_CB, blockbegin_handle);
		blockbegin_handle = DECAF_NULL_HANDLE;
	}

}

static mon_cmd_t hookapitests_term_cmds[] = {
#include "plugin_cmds.h"
		{ NULL, NULL, }, };

plugin_interface_t* init_plugin(void) {
	hookapitests_interface.mon_cmds = hookapitests_term_cmds;
	hookapitests_interface.plugin_cleanup = &hookapitests_cleanup;

	//initialize the plugin
	hookapitests_init();
	return (&hookapitests_interface);
}

