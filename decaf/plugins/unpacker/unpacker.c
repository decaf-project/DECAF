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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "config.h"

#include "DECAF_main.h"
#include "DECAF_callback.h"
#include "DECAF_target.h"
//change end
//#include "shared/read_linux.h"
//#include "shared/VMI.h"
#include "shared/vmi_callback.h"
#include "shared/hookapi.h"
#include "unpacker.h"
#include "mem_mark.h"


#define size_to_mask(size) ((1u << (size)) - 1u) //size<=4
static plugin_interface_t unpacker_interface;
//change begin
/* Callback handles */
DECAF_Handle block_begin_cb_handle = 0;
DECAF_Handle mem_write_cb_handle=0;
DECAF_Handle proc_loadmodule_cb_handle=0;
DECAF_Handle proc_loadmainmodule_cb_handle=0;
DECAF_Handle proc_processend_cb_handle=0;
//change end
static FILE *unpacker_log = NULL;


static int max_rounds = 100;
static int cur_version = 1;
target_ulong unpack_cr3 = 0;
char unpack_basename[256] = "";
clock_t start;
uint32_t monitored_pid = 0;

//change to
static mon_cmd_t unpacker_term_cmds[] = {
		{
				.name="set_max_unpack_rounds",
				.args_type="rounds:i",
				.mhandler=do_set_max_unpack_rounds,
				.params="rounds",
				.help="Set the maximum unpacking rounds (100 by default)",
		},
		{
				.name="trace_by_name",
				.args_type="filename:s",
				.mhandler=do_trace_process,
				.params="filename",
				.help="specify the process name",

		},
		{
				.name="stop_unpack",
				.args_type="",
				.mhandler=do_stop_unpack,
				.params="",
				.help="Stop the unpacking process",

		},
		{
				.name="linux_ps",
				.args_type="",
				.mhandler=do_linux_ps,
				.params="",
				.help="List the processes on linux guest system",

		},
		{
				.name="guest_ps",
				.args_type="",
				.mhandler=do_guest_procs,
				.params="",
				.help="List the processes on guest system",

		},
};
void do_set_max_unpack_rounds(Monitor *mon, const QDict *qdict)
{
	const int rounds=qdict_get_int(qdict,"rounds");
	  if (rounds <= 0) {
	    DECAF_printf("Unpack rounds has to be greater than 0!\n");
	    return;
	  }
	  max_rounds = rounds;
}
//static inline const char *get_basename(const char *path)
static inline const char *get_basename(const char *path)
{
  int i = strlen(path) - 1;
  for (; i >= 0; i--)
    if (path[i] == '/')
      return &path[i + 1];
  return path;
}
void do_trace_process(Monitor *mon, const QDict *qdict)
{
	const char *filename=qdict_get_str(qdict,"filename");
	const char *basename=get_basename(filename);
	if(!basename){
		DECAF_printf("cannot get basename\n");
		return;
	}
	strncpy(unpack_basename,filename,256);
	unpack_basename[255]='\0';
	DECAF_printf("Waiting for process %s(case sensitive to start)\n",unpack_basename);
	return;

}
void do_stop_unpack(Monitor *mon, const QDict *qdict)
{
	  unpack_cr3 = 0;
	  unpack_basename[0] = 0;
	  //For DECAF taint check need to be completed
	  //  mem_mark_cleanup();
	  cur_version = 1;
}
void do_linux_ps(Monitor *mon, const QDict *qdict)
{

}
void do_guest_procs(Monitor *mon, const QDict *qdict)
{

}

//end change




/*
	1. Search backward of virtual address from EIP for clean pages, and dump them
	2. Search forward of virtual address from EIP for clean pages, and dump them
	3. Search stops when we meet a clean page or all tainted bytes are already dumpped
	4. When dump a page, set "dumpped" flag to 1
*/
//will change it !!!!!!!!!!!!!!!!!!!!!!
static void dump_unpacked_code()
{
  uint32_t start_va, end_va, i, j, eip, cr3;
  uint8_t buf[TARGET_PAGE_SIZE];
  char filename[128];
  taint_record_t records[4];

  eip = DECAF_getPC(cpu_single_env);
  cr3 = DECAF_getPGD(cpu_single_env);

  //we just dump one page
  start_va = (eip & TARGET_PAGE_MASK);
  end_va = start_va + TARGET_PAGE_SIZE;
  sprintf(filename, "dump-%d-%08x-%08x-%08x.bin\n", cur_version, eip, start_va, end_va + TARGET_PAGE_SIZE - 1);
  FILE *fp = fopen(filename, "wb");
  assert(fp);

  for (i = 0; i < 4; i++) {
    records[i].version = cur_version;
  }

  for (i = start_va; i < end_va; i += TARGET_PAGE_SIZE) {
    bzero(buf, sizeof(buf));
    for (j = 0; j < TARGET_PAGE_SIZE; j+=4) {
    	set_mem_mark(i+j,4,0);
    	//taintcheck_taint_virtmem(i+j, 4, 0, records);//Need change for DECAF
    }
    if(DECAF_memory_rw(NULL,i,buf,TARGET_PAGE_SIZE,0)<0)
    	DECAF_printf(default_mon,"Cannot dump this page %08x!!! \n", i);
    else
    	fwrite(buf, TARGET_PAGE_SIZE, 1, fp);
  }
  fclose(fp);
  // printf("OK after taintcheck_taint_virtmem()\n");
}

int inContext = 0;
static void unpacker_block_begin(DECAF_Callback_Params*dcp)
{
	CPUState *env = dcp->bb.env;
	/*
	 * check current instruction:
	 * if it belongs to the examined process, and
	 * if it is clean, dump the region
	*/
	uint32_t eip, cr3; 
	if(unpack_basename[0] == '\0')
		return ;

	cr3 = DECAF_getPGD(env);
	if(unpack_cr3 == 0) {
		char current_proc[256];
		uint32_t pid;

		VMI_find_process_by_cr3_c(cr3, current_proc, sizeof(current_proc), &pid);
		if(strcasecmp(current_proc, unpack_basename) != 0)
		  return ;
		unpack_cr3 = cr3;
	}
	inContext = (unpack_cr3 == cr3) && (!DECAF_is_in_kernel(dcp->ib.env)); 
	eip = DECAF_getPC(env);
	if (!inContext)
		return ;

/* Changed by Aravind: arprakas@syr.edu */
    	uint64_t mybitmap=0;
    	mybitmap=check_mem_mark(eip,1);
    	if(mybitmap>0){
    		DECAF_printf("will dump this region: eip=%08x \n", eip);
    		DECAF_printf("Suspicious activity!\n");
    		fprintf(unpacker_log, "suspcious instruction: eip=%08x \n", eip);
    		fflush(unpacker_log);
    		dump_unpacked_code();
    		cur_version++;
    	}

    	return ;
}
//change add
static void unpacker_module_loaded(VMI_Callback_Params *pcp)
//change end
{
	//uint32_t pid=pcp->lm.pid;
	//char *name=pcp->lm.name;
	uint32_t base=pcp->lm.base;
	uint32_t size=pcp->lm.size;
    uint32_t virt_page, i;
    for (virt_page = base; virt_page < base + size;
    		virt_page += TARGET_PAGE_SIZE) {
      for (i = 0; i < TARGET_PAGE_SIZE; i+=4) {
    	  //taint check needed
    	  set_mem_mark(i+virt_page,4,0);
      }
   }
   fprintf(unpacker_log, "clean virt_page=%08x, size = %d \n", virt_page, size);
}


static void unpacker_mem_write(DECAF_Callback_Params*dcp)
{
	// DECAF change
	uint32_t virt_addr;//,phys_addr;
	int size;
	//phys_addr=dcp->mw.phy_addr;
	virt_addr=dcp->mw.vaddr;
	size=dcp->mw.dt;
	//end
    /* Changed by Aravind: arprakas@syr.edu */
	if(inContext) {
		set_mem_mark(virt_addr,size,(1<<size)-1);
	} else {
	//clean memory 
		//taintcheck_clean_memory(phys_addr, size);  //Need change for DECAF
		set_mem_mark(virt_addr,size,0);
	}
/*	END	*/
}
void unregister_callbacks()
{
	DECAF_printf(default_mon,"Unregister_callbacks\n");
	if(block_begin_cb_handle){
		DECAF_printf(default_mon,"DECAF_unregister_callback(DECAF_BLOCK_BEGIN_CB,block_begin_cb_handle);\n");
		DECAF_unregister_callback(DECAF_BLOCK_BEGIN_CB,block_begin_cb_handle);
	}
	if(mem_write_cb_handle){
		DECAF_printf(default_mon,"DECAF_unregister_callback(DECAF_MEM_WRITE_CB,mem_write_cb_handle);\n");
		DECAF_unregister_callback(DECAF_MEM_WRITE_CB,mem_write_cb_handle);
	}
	if(proc_loadmodule_cb_handle){
		DECAF_printf(default_mon,"VMI_unregister_callback(VMI_LOADMODULE_CB)\n");
		VMI_unregister_callback(VMI_LOADMODULE_CB,proc_loadmodule_cb_handle);
	}
	if(proc_loadmainmodule_cb_handle){
		DECAF_printf(default_mon,"VMI_unregister_callback(VMI_CREATEPROC_CB)\n");
		VMI_unregister_callback(VMI_CREATEPROC_CB,proc_loadmainmodule_cb_handle);
	}
	if(proc_processend_cb_handle){
		DECAF_printf(default_mon,"VMI_unregister_callback(VMI_REMOVEPROC_CB)\n");
		VMI_unregister_callback(VMI_REMOVEPROC_CB,proc_processend_cb_handle);
	}
}
static void unpacker_cleanup()
{
	//DECAF change
	  //clean memory

	unregister_callbacks();
	//DECAF end
	if(unpacker_log) fclose(unpacker_log);
}


static void unpacker_loadmainmodule_notify(VMI_Callback_Params *vcp)
{
		uint32_t pid=vcp->cp.pid;
		char *name=vcp->cp.name;

		if(unpack_basename[0] != 0) {
			if(strcasecmp(name, unpack_basename)==0) {
				DECAF_printf(default_mon,"loadmainmodule_notify called, %s\n", name);

				monitored_pid = pid;
				//unpack_cr3=find_cr3(pid);
				unpack_cr3 = VMI_find_cr3_by_pid_c(pid);
				if(!block_begin_cb_handle){
					block_begin_cb_handle=DECAF_register_callback(DECAF_BLOCK_BEGIN_CB,unpacker_block_begin,NULL);
					DECAF_printf(default_mon,"DECAF_register_callback(DECAF_BLOCK_BEGIN_CB) pid=%d\n",pid);
				}
				if(!mem_write_cb_handle){
					mem_write_cb_handle=DECAF_register_callback(DECAF_MEM_WRITE_CB,unpacker_mem_write,NULL);
					DECAF_printf(default_mon,"DECAF_register_callback(DECAF_MEM_WRITE_CB) pid=%d\n",pid);
				}
				start = clock();
			}
		}
}

static void unpacker_removeproc_notify(VMI_Callback_Params *vcp)
{
	uint32_t pid=vcp->rp.pid;
  if(monitored_pid != 0 && monitored_pid == pid) {

	  DECAF_printf(default_mon,"unpacker_removeproc_notify pid=%d\n", pid);
	  //change begin
	  do_stop_unpack(NULL,NULL);
	  //change end
	  monitored_pid = 0;
	  printf("Unpacker: Time taken: %f\n", ((double)clock() - start)/CLOCKS_PER_SEC);
  }
}

plugin_interface_t * init_plugin()
{
  unpack_basename[0] = '\0';
  if (!(unpacker_log = fopen("unpack.log", "w"))) {
	printf("Unable to open unpack.log for writing!\n");
	return NULL;
  }
  mem_mark_init();

  //change to
  unpacker_interface.mon_cmds=unpacker_term_cmds;
  unpacker_interface.plugin_cleanup=unpacker_cleanup;
  proc_loadmodule_cb_handle=VMI_register_callback(VMI_LOADMODULE_CB,unpacker_module_loaded,&should_monitor);
  proc_loadmainmodule_cb_handle=VMI_register_callback(VMI_CREATEPROC_CB,unpacker_loadmainmodule_notify,&should_monitor);
  proc_processend_cb_handle=VMI_register_callback(VMI_REMOVEPROC_CB,unpacker_removeproc_notify,&should_monitor);
    //change end

  return &unpacker_interface;
}


