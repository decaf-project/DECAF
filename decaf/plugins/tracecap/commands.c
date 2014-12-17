/* 
 Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
 All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.

 Author: Juan Caballero <jcaballero@cmu.edu>
 Zhenkai Liang <liangzk@comp.nus.edu.sg>
 */
#include "config.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "shared/tainting/taintcheck_opt.h" // AWH
#include "shared/tainting/tainting.h"
#include "DECAF_main.h"
#include "DECAF_target.h"
#include "DECAF_callback.h"
#include "vmi_callback.h"
#include "conf.h"
// AWH #include "libfstools.h"
#include "slirp/slirp.h"

#include "reg_ids.h"

#include "conditions.h"

#include "network.h"

#include "tracecap.h"

#include "hookapi.h"
#include "function_map.h"
#include "utils/Output.h" // AWH
#include "vmi_c_wrapper.h" // AWH
#include "qemu-timer.h" // AWH
#include "operandinfo.h" // AWH
#include "readwrite.h" // AWH
#include "trackproc.h" // AWH

/* Plugin interface */
static plugin_interface_t tracing_interface;
//to keep consistent
static DECAF_Handle block_begin_cb_handle;
static DECAF_Handle insn_begin_cb_handle;
static DECAF_Handle insn_end_cb_handle;
static DECAF_Handle nic_rec_cb_handle;
static DECAF_Handle nic_send_cb_handle;
static DECAF_Handle keystroke_cb_handle;
static DECAF_Handle removeproc_handle;
static DECAF_Handle loadmainmodule_handle;
static DECAF_Handle loadmodule_handle;

static DECAF_Handle check_eip_handle;

int io_logging_initialized = 0;

char current_mod[32] = "";
char current_proc[32] = "";

/* Origin and offset set by the taint_sendkey monitor command */
#ifdef CONFIG_TCG_TAINT
//static int taint_sendkey_origin = 0;
//static int taint_sendkey_offset = 0;
static int taint_key_enabled=0;
#endif

/* Current thread id */
uint32_t current_tid = 0;

static char tracefile[256];

uint32_t should_trace_all_kernel = 0;

// AWH - Changed to use new mon_cmd_t datatype
static mon_cmd_t tracing_info_cmds[] = { { NULL, NULL, }, };

// AWH - Make internal version of these monitor commands so that we can
// call them programatically from other functions without create QDict
//static void do_load_hooks_internal(const char *hooks_dirname,
//		const char *plugins_filename);
static void do_tracing_internal(uint32_t pid, const char *filename);
static void do_tracing_by_name_internal(const char *progname,
		const char *filename);

// AWH - Forward declarations of other funcs used below
static int tracing_init(void);
static void tracing_cleanup(void);
/* target-i386/op_helper.c */
extern uint32_t helper_cc_compute_all(int op);

#ifdef CONFIG_TCG_TAINT

//check EIP tainted

static void check_eip(DECAF_Callback_Params* params)
{
	if(params->ec.target_eip_taint)
	printf("CHECK_EIP : SOURCE: 0x%08x TARGET: 0x%08x  TAINT_VALUE: 0x%08x \n", params->ec.source_eip, params->ec.target_eip, params->ec.target_eip_taint);
}

static void tracing_send_keystroke(DECAF_Callback_Params *params)
{
	/* If not tracing, return */
	if (tracepid == 0)
	return;
	if(!taint_key_enabled)
	return;

	int keycode=params->ks.keycode;
	uint32_t *taint_mark=params->ks.taint_mark;
	*taint_mark=taint_key_enabled;
	taint_key_enabled=0;
	printf("taint keystroke %d \n ",keycode);
}

// void do_taint_sendkey(const char *key, int taint_origin, int taint_offset)
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
#endif //CONFIG_TCG_TAINT

static int tracing_init(void) {
	int err = 0;

	procname_clear();
	// Parse configuration file
	err = check_ini(ini_main_default_filename);
	if (err) {
		DECAF_printf( "Could not find INI file: %s\n"
				"Use the command 'load_config <filename> to provide it.\n",
				ini_main_default_filename);
	}
	return 0;
}

static void tracing_cleanup(void) {
	DECAF_stop_vm();

	if (removeproc_handle != DECAF_NULL_HANDLE)
		VMI_unregister_callback(VMI_REMOVEPROC_CB, removeproc_handle);
	if (loadmainmodule_handle != DECAF_NULL_HANDLE)
		VMI_unregister_callback(VMI_CREATEPROC_CB, loadmainmodule_handle);
	if (loadmodule_handle != DECAF_NULL_HANDLE)
		VMI_unregister_callback(VMI_LOADMODULE_CB, loadmodule_handle);
	if (block_begin_cb_handle)
		DECAF_unregister_callback(DECAF_BLOCK_BEGIN_CB, block_begin_cb_handle);
	if (insn_begin_cb_handle)
		DECAF_unregister_callback(DECAF_INSN_BEGIN_CB, insn_begin_cb_handle);
	if (insn_end_cb_handle)
		DECAF_unregister_callback(DECAF_INSN_END_CB, insn_end_cb_handle);
	if (nic_rec_cb_handle)
		DECAF_unregister_callback(DECAF_NIC_REC_CB, nic_rec_cb_handle);
	if (nic_send_cb_handle)
		DECAF_unregister_callback(DECAF_NIC_SEND_CB, nic_send_cb_handle);
	if (keystroke_cb_handle)
		DECAF_unregister_callback(DECAF_KEYSTROKE_CB, keystroke_cb_handle);
	if (check_eip_handle)
		DECAF_unregister_callback(DECAF_EIP_CHECK_CB, check_eip_handle);

	DECAF_start_vm();
}

void do_tracing_stop(Monitor *mon) {
	if (io_logging_initialized) {
		//TODO:fix this -hu
		//netwk_monitor_cleanup();
		io_logging_initialized = 0;
	}
	tracing_stop();
}

static void do_tracing_internal(uint32_t pid, const char *filename) {
	/* if pid = 0, stop trace */
	if (0 == pid)
		tracing_stop();
	else {
		if (!io_logging_initialized) {
			//TODO: fix this -hu
			//netwk_monitor_init(filename);
			io_logging_initialized = 1;
		}
		int retval = tracing_start(pid, filename);
		if (retval < 0)
			DECAF_printf( "Unable to open log file '%s'\n",
					filename);
	}

	/* Print configuration variables */
	//print_conf_vars();
}

void do_tracing(Monitor *mon, const QDict *qdict)
{
  uint32_t pid = qdict_get_int(qdict, "pid");
  const char *filename = qdict_get_str(qdict, "filepath");
  char proc_name[512];
  uint32_t cr3;
  if( VMI_find_process_by_pid_c(pid, proc_name, 512, &cr3)!= -1){
  	  	  procname_set(proc_name);
  		strncpy(tracefile, filename, 256);
  		do_tracing_internal(pid, filename);
  		trackproc_start(pid);
  }
  else
	  DECAF_printf("Unable to find process %d \n", pid);
}

void set_trace_kernel(Monitor *mon, const QDict *qdict) {
	const char *filename = qdict_get_str(qdict, "filepath");

	should_trace_all_kernel = 1;
	do_tracing_internal(-1, filename);

}
void do_tracing_by_name(Monitor *mon, const QDict *qdict) {
	do_tracing_by_name_internal(qdict_get_str(qdict, "name"),
			qdict_get_str(qdict, "filepath"));

}

static void do_tracing_by_name_internal(const char *progname,
		const char *filename) {
	/* If process already running, start tracing */
	uint32_t pid = VMI_find_pid_by_name_c(progname);
	uint32_t minus_one = (uint32_t)(-1);
	if (pid != minus_one) {
		procname_set(progname);
		strncpy(tracefile, filename, 256);
		do_tracing_internal(pid, filename);
		trackproc_start(pid);
		return;
	}

	/* Otherwise, start monitoring for process start */
	procname_set(progname);
	strncpy(tracefile, filename, 256);
	DECAF_printf( "Waiting for process %s to start\n", progname);

}

void tracing_insn_begin(DECAF_Callback_Params* params) {
	CPUState* env = NULL;
#if 0 // AWH
	if (params != NULL) {
		env = params->ib.env;
	}
#else
	if (!params) return;
	env = params->ib.env;
#endif // AWH
	if (DECAF_is_in_kernel(env) && should_trace_all_kernel)
		goto TRACE_KERNEL;

	/* If tracing start condition not satisified, or not tracing return */
	if ((!tracing_start_condition) || (tracepid == 0))
		return;

	/* If not tracing kernel and kernel instruction , return */
	if (DECAF_is_in_kernel(env) && !tracing_kernel())
		return;

	if(DECAF_getPGD(env) != tracecr3 && (!DECAF_is_in_kernel(env)))
		return;

	TRACE_KERNEL:
	cpu_disable_ticks();
	/* Get thread id */
	current_tid = VMI_get_current_tid_c(env);

	/* Clear flags before processing instruction */

	// Flag to be set if the instruction is written
	insn_already_written = 0;

	// Flag to be set if instruction encounters a page fault
	// NOTE: currently not being used. Tracing uses it to avoid logging twice
	// these instructions, but was missing some
	has_page_fault = 0;

	// Flag to be set if instruction accesses user memory
	access_user_mem = 0;

	/* Disassemble the instruction */
	insn_tainted = 0;
	if (skip_decode_address == 0) {
		decode_address(/* AWH cpu_single_*/ env->eip, &eh);
	}
	cpu_enable_ticks();

}

void tracing_insn_end(DECAF_Callback_Params* params) {
	CPUState * env;

	/* If not decoding, return */
	if (skip_decode_address || !params)
		return;
	env = params->ie.env;
	if (DECAF_is_in_kernel(env) && should_trace_all_kernel)
		goto TRACE_KERNEL;
	/* If tracing start condition not satisified, or not tracing return */
	if ((!tracing_start_condition) || (tracepid == 0))
		return;

	/* If not tracing kernel and kernel instruction , return */
	if (DECAF_is_in_kernel(env) && !tracing_kernel())
		return;

	/* If partially tracing kernel but did not access user memory, return */
	if (DECAF_is_in_kernel(env)) {
		if (tracing_kernel_partial() && (!access_user_mem))
			return;
#ifdef CONFIG_TCG_TAINT
		if (tracing_kernel_tainted() && (!insn_tainted))
		return;
#endif	  
	}
	if(DECAF_getPGD(/* AWH cpu_single_*/ env) != tracecr3 && (!DECAF_is_in_kernel(env)))
		return;
	TRACE_KERNEL:

	/* Update the eflags */
	// eh.eflags = *DECAF_cpu_eflags;
#if 0 // AWH
	eh.eflags = compute_eflag();
#else
#define DF_MASK	0x00000400
	eh.eflags = env->eflags | helper_cc_compute_all(env->cc_op) |
		(env->df & DF_MASK);
#endif // AWH
	//for test
//check taint value after insn is executed

	int k, j;
	for (k = 0; k < eh.num_operands; k++) {
		if (eh.operand[k].length)
			set_operand_data(&(eh.operand[k]), 0);
		for (j = 0; j < MAX_NUM_MEMREGS; j++) {
			if (eh.memregs[k][j].length)
				set_operand_data(&(eh.memregs[k][j]), 0);
		}

	}

	eh.df = (/* AWH cpu_single_*/ env->df == 1) ? 0x1 : 0xff;

	//TODO:fix this -hu
	/* Clear eh.tp if inside a function hook */
//  if (get_st(current_tid) > 0) eh.tp = TP_NONE;
//  else {
	/* Update eh.tp if rep instruction */
	if ((eh.operand[2].usage == counter) && (eh.operand[2].tainted_begin != 0))
		eh.tp = TP_REP_COUNTER;

	/* Updated eh.tp if sysenter */
	else if ((eh.rawbytes[0] == 0x0f) && (eh.rawbytes[1] == 0x34))
		eh.tp = TP_SYSENTER;
	//}

	/* Split written operands if requested */
	if (conf_write_ops_at_insn_end) {
		update_written_operands(&eh);
	}

	/* If not writing to trace, or instruction already written, return */
	if (skip_trace_write || (insn_already_written == 1))
		return;

	/* Write the disassembled instruction to the trace */
	if (tracing_tainted_only()) {
#ifdef CONFIG_TCG_TAINT
		if (insn_tainted)
		write_insn(tracelog,&eh);
#endif      
	} else {
		if (conf_trace_only_after_first_taint) {
			if ((received_tainted_data == 1) && (has_page_fault == 0)) {
				write_insn(tracelog, &eh);
			}
		} else {
			if (has_page_fault == 0)
				write_insn(tracelog, &eh);
		}
	}

	/* If first trace instruction, save state if requested */
	if ((tstats.insn_counter_traced == 1) && conf_save_state_at_trace_start) {
		char prestatename[128];
		snprintf(prestatename, 128, "%s.pre", tracename_p);
//TODO: fix this -hu
		//   int err = save_state_by_cr3(tracecr3, prestatename);
		//  if (err) {
		//    DECAF_printf( "Could not save state");
		// }
	}

	/* Record the thread ID of the first instruction in the trace, if needed */
	if (tracing_single_thread_only()) {
		if (tid_to_trace == -1 && insn_already_written == 1) {
			// If tid_to_trace is not -1, we record trace only the given thread id.
			tid_to_trace = current_tid;
		}
	}

}

static mon_cmd_t tracing_term_cmds[] = {
#include "plugin_cmds.h"
		{ NULL, NULL, }, };

int monitored_pid = 0;

static void my_loadmainmodule_notify(VMI_Callback_Params * params) {

	char *name = params->cp.name;
	if (procname_is_set()) {
		if (procname_match(name)) {

			do_tracing_internal(params->cp.pid, tracefile);
			trackproc_start(params->cp.pid);
			DECAF_printf( "Tracing %s\n", procname_get());
			procname_clear();
		}
	}

}

static void my_loadmodule_notify(VMI_Callback_Params *params) {
	char *name = params->lm.name;

	if (modname_is_set()) {
		if (modname_match(name)
				&& (decaf_plugin->monitored_cr3 == cpu_single_env->cr[3])) {
			tracing_start_condition = 1;
			modname_clear();
		}
	}
}
static void my_removeproc_notify(VMI_Callback_Params *params) {
	if (tracepid == params->rp.pid)
		tracing_stop();
}

extern target_ulong VMI_guest_kernel_base;
plugin_interface_t * init_plugin() {

	if (0x80000000 == VMI_guest_kernel_base)
		comparestring = strcasecmp;
	else
		comparestring = strcmp;

	tracing_interface.plugin_cleanup = tracing_cleanup;
	tracing_interface.mon_cmds = tracing_term_cmds;
	tracing_interface.info_cmds = tracing_info_cmds;

	//for now, receive block begin callback globally
	DECAF_stop_vm();

	// register for insn begin/end
	insn_begin_cb_handle = DECAF_register_callback(DECAF_INSN_BEGIN_CB,
			tracing_insn_begin, &should_monitor);

	insn_end_cb_handle = DECAF_register_callback(DECAF_INSN_END_CB,
			tracing_insn_end, &should_monitor);
#ifdef CONFIG_TCG_TAINT
	//  //register taint nic callback
	nic_rec_cb_handle = DECAF_register_callback(DECAF_NIC_REC_CB,
			tracing_nic_recv, NULL);
	nic_send_cb_handle = DECAF_register_callback(DECAF_NIC_SEND_CB,
			tracing_nic_send, NULL);
	printf("register nic callback \n");

	//check EIP tainted
	check_eip_handle = DECAF_register_callback(DECAF_EIP_CHECK_CB, check_eip, NULL);
	printf("register eip check callback\n");
#endif /*CONFIG_TCG_TAINT*/


	DECAF_start_vm();
	removeproc_handle = VMI_register_callback(VMI_REMOVEPROC_CB,
			my_removeproc_notify, NULL);
	loadmainmodule_handle = VMI_register_callback(VMI_CREATEPROC_CB,
			my_loadmainmodule_notify, NULL);
	loadmodule_handle = VMI_register_callback(VMI_LOADMODULE_CB,
			my_loadmodule_notify, NULL);
	tracing_init();
	return &tracing_interface;
}
