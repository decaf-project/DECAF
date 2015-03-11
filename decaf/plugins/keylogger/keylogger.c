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
#include "DECAF_target.h"
#include "hookapi.h"
#include "DECAF_callback.h"

#include "utils/Output.h"
#include "function_map.h"
#include "vmi_callback.h"
#include "vmi_c_wrapper.h"
//basic stub for plugins
static plugin_interface_t keylogger_interface;
static int taint_key_enabled=0;

DECAF_Handle keystroke_cb_handle = DECAF_NULL_HANDLE;
DECAF_Handle handle_write_taint_mem = DECAF_NULL_HANDLE;
DECAF_Handle handle_read_taint_mem = DECAF_NULL_HANDLE;
DECAF_Handle handle_block_end_cb = DECAF_NULL_HANDLE;
FILE * keylogger_log=DECAF_NULL_HANDLE;

#define MAX_STACK_SIZE 5000
char modname_t[512];
char func_name_t[512];
uint32_t sys_call_ret_stack[MAX_STACK_SIZE];
uint32_t sys_call_entry_stack[MAX_STACK_SIZE];
uint32_t cr3_stack[MAX_STACK_SIZE];
uint32_t stack_top = 0;
void check_call(DECAF_Callback_Params *param)
{
	CPUState *env=param->be.env;
	if(env == NULL)
	return;
	target_ulong pc = param->be.next_pc;
	target_ulong cr3 = DECAF_getPGD(env) ;

	if(stack_top == MAX_STACK_SIZE)
	{
     //if the stack reaches to the max size, we ignore the data from stack bottom to MAX_STACK_SIZE/10
		memcpy(sys_call_ret_stack,&sys_call_ret_stack[MAX_STACK_SIZE/10],MAX_STACK_SIZE-MAX_STACK_SIZE/10);
		memcpy(sys_call_entry_stack,&sys_call_entry_stack[MAX_STACK_SIZE/10],MAX_STACK_SIZE-MAX_STACK_SIZE/10);
		memcpy(cr3_stack,&cr3_stack[MAX_STACK_SIZE/10],MAX_STACK_SIZE-MAX_STACK_SIZE/10);
		stack_top = MAX_STACK_SIZE-MAX_STACK_SIZE/10;
		return;
	}
	if(funcmap_get_name_c(pc, cr3, modname_t, func_name_t))
	{
		DECAF_read_mem(env,env->regs[R_ESP],4,&sys_call_ret_stack[stack_top]);
		sys_call_entry_stack[stack_top] = pc;
		cr3_stack[stack_top] = cr3;
		stack_top++;

	}



}
void check_ret(DECAF_Callback_Params *param)
{
	if(!stack_top)
		return;
	if(param->be.next_pc == sys_call_ret_stack[stack_top-1])
	{
		stack_top--;
	}
}
void do_block_end_cb(DECAF_Callback_Params *param)
{
	unsigned char insn_buf[2];
	int is_call = 0, is_ret = 0;
	int b;
	DECAF_read_mem(param->be.env,param->be.cur_pc,sizeof(char)*2,insn_buf);

	switch(insn_buf[0]) {
		case 0x9a:
		case 0xe8:
		is_call = 1;
		break;
		case 0xff:
		b = (insn_buf[1]>>3) & 7;
		if(b==2 || b==3)
		is_call = 1;
		break;

		case 0xc2:
		case 0xc3:
		case 0xca:
		case 0xcb:
		is_ret = 1;
		break;
		default: break;
	}

	/*
	 * Handle both the call and the return
	 */
	if (is_call)
	check_call(param);
	else if (is_ret)
	check_ret(param);

}
void do_read_taint_mem(DECAF_Callback_Params *param)
{
	uint32_t eip=DECAF_getPC(cpu_single_env);
	uint32_t cr3= DECAF_getPGD(cpu_single_env);
	char name[128];
	tmodinfo_t dm;// (tmodinfo_t *) malloc(sizeof(tmodinfo_t));
	if(VMI_locate_module_c(eip,cr3, name, &dm) == -1)
	{
		strcpy(name, "<None>");
		bzero(&dm, sizeof(dm));
	}
	if(stack_top)
	{
		if(cr3 == cr3_stack[stack_top-1])
			funcmap_get_name_c(sys_call_entry_stack[stack_top-1], cr3, modname_t, func_name_t);
		else {
			memset(modname_t, 0, 512);
			memset(func_name_t, 0, 512);
		}
	}
	else {
		memset(modname_t, 0, 512);
		memset(func_name_t, 0, 512);
	}
	if(param->rt.size <=4)
		fprintf(keylogger_log,"%s   \t 0 \t 0x%08x \t\t 0x%08x \t %d      0x%08x  0x%08x %15s    \t%s\t%s\n",
			name, param->rt.vaddr, param->rt.paddr, param->rt.size,*((uint32_t *)param->rt.taint_info), eip, dm.name,modname_t,func_name_t);
	else
		fprintf(keylogger_log,"%s   \t 0 \t 0x%08x \t\t 0x%08x \t %d      0x%16x  0x%08x %15s     \t%s\t%s\n",
					name, param->rt.vaddr, param->rt.paddr, param->rt.size,*((uint32_t *)param->rt.taint_info), eip, dm.name,  modname_t,func_name_t);


}


void do_write_taint_mem(DECAF_Callback_Params *param)
{
	uint32_t eip= DECAF_getPC(cpu_single_env);
	uint32_t cr3= DECAF_getPGD(cpu_single_env);
	char name[128];
	tmodinfo_t  dm;// (tmodinfo_t *) malloc(sizeof(tmodinfo_t));
	if(VMI_locate_module_c(eip,cr3, name, &dm) == -1)
	{
		strcpy(name, "<None>");
		bzero(&dm, sizeof(dm));
	}

	if(stack_top)
	{
		if(cr3 == cr3_stack[stack_top-1])
			funcmap_get_name_c(sys_call_entry_stack[stack_top-1], cr3, modname_t, func_name_t);
		else {
			memset(modname_t, 0, 512);
			memset(func_name_t, 0, 512);
		}
	}
	else {
		memset(modname_t, 0, 512);
		memset(func_name_t, 0, 512);
	}

	//fprintf(keylogger_log,"%s    1  0x%08x  0x%08x  %d   0x%08x %s   0x%08x  %d    %s\n",
	//		name, param->rt.vaddr, param->rt.paddr, param->rt.size, eip, dm->name, dm->base, dm->size, dm->fullname);
	if(param->rt.size <=4)
			fprintf(keylogger_log,"%s   \t 1 \t 0x%08x \t\t 0x%08x \t %d      0x%08x  0x%08x %15s   \t%s\t%s\n",
				name, param->rt.vaddr, param->rt.paddr, param->rt.size,*((uint32_t *)param->rt.taint_info), eip, dm.name,modname_t,func_name_t);
		else
			fprintf(keylogger_log,"%s   \t 1 \t 0x%08x \t\t 0x%08x \t %d      0x%16x  0x%08x %15s    \t%s\t%s\n",
						name, param->rt.vaddr, param->rt.paddr, param->rt.size,*((uint32_t *)param->rt.taint_info), eip, dm.name, modname_t,func_name_t);



}

static void tracing_send_keystroke(DECAF_Callback_Params *params)
{
  if(!taint_key_enabled)
	  return;

  int keycode=params->ks.keycode;
  uint32_t *taint_mark=params->ks.taint_mark;
  *taint_mark=taint_key_enabled;
  taint_key_enabled=0;
  printf("taint keystroke %d \n ",keycode);
}

void do_taint_sendkey(Monitor *mon, const QDict *qdict)
{
  // Set the origin and offset for the callback
  if(qdict_haskey(qdict, "key"))
  {
	//register keystroke callback
     taint_key_enabled=1;
		if (!keystroke_cb_handle)
			keystroke_cb_handle = DECAF_register_callback(DECAF_KEYSTROKE_CB,
					tracing_send_keystroke, &taint_key_enabled);
    // Send the key
    do_send_key(qdict_get_str(qdict, "key"));

  }
  else
    monitor_printf(mon, "taint_sendkey command is malformed\n");
}

void keylogger_cleanup()
{
	  if(keylogger_log)
			fclose(keylogger_log);
		if(handle_read_taint_mem)
			DECAF_unregister_callback(DECAF_READ_TAINTMEM_CB,handle_read_taint_mem);
		if(handle_write_taint_mem)
			DECAF_unregister_callback(DECAF_WRITE_TAINTMEM_CB,handle_write_taint_mem);
		if(handle_block_end_cb)
			DECAF_unregisterOptimizedBlockEndCallback(handle_block_end_cb);
		handle_read_taint_mem = DECAF_NULL_HANDLE;
		handle_write_taint_mem = DECAF_NULL_HANDLE;
		keylogger_log = NULL;
		handle_block_end_cb = DECAF_NULL_HANDLE;

}
void do_enable_keylogger_check( Monitor *mon, const QDict *qdict)
{
	const char *tracefile_t = qdict_get_str(qdict, "tracefile");
	keylogger_log= fopen(tracefile_t,"w");
	if(!keylogger_log)
	{
		DECAF_printf("the %s can not be open or created !!",tracefile_t);
		return;
	}
	fprintf(keylogger_log,"Process Read(0)/Write(1) vaddOfTaintedMem   paddrOfTaintedMem    Size   "
			"TaintInfo   CurEIP \t ModuleName   \t CallerModuleName \t CallerSystemCall\n");
	if(!handle_read_taint_mem)
		handle_read_taint_mem = DECAF_register_callback(DECAF_READ_TAINTMEM_CB,do_read_taint_mem,NULL);
	if(!handle_write_taint_mem)
		handle_write_taint_mem = DECAF_register_callback(DECAF_WRITE_TAINTMEM_CB,do_write_taint_mem,NULL);
	if(!handle_block_end_cb)
		handle_block_end_cb =  DECAF_registerOptimizedBlockEndCallback(
				do_block_end_cb, NULL, INV_ADDR, INV_ADDR);

}
void do_disable_keylogger_check( Monitor *mon, const QDict *qdict)
{
	keylogger_cleanup();
	DECAF_printf("disable taintmodule check successfully \n");
}

static mon_cmd_t keylogger_term_cmds[] = {
#include "plugin_cmds.h"
		{ NULL, NULL, }, };

plugin_interface_t* init_plugin(void) {
	keylogger_interface.mon_cmds = keylogger_term_cmds;
	keylogger_interface.plugin_cleanup = &keylogger_cleanup;

	//initialize the plugin

	return (&keylogger_interface);
}

