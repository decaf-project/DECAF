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
#include <string>
#include <list>
#include "qemu-common.h"
#include "hw/hw.h"
#include "DECAF_main.h"
#include "DECAF_target.h"
#include "hookapi.h"
#include "read_linux.h"
#include "shared/function_map.h"
#include "vmi_callback.h"
#include "shared/DECAF_main.h"
#include "shared/utils/SimpleCallback.h"
#include "vmi.h"
#include "windows_vmi.h"
#include "shared/DECAF_types.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <tr1/unordered_map>
#include <stdlib.h>


#define GUEST_MESSAGE_LEN 512
#define GUEST_MESSAGE_LEN_MINUS_ONE 511
#define MAX_MODULE_COUNT 500
extern FILE *guestlog;
void parse_process(const char *log);
void parse_module(const char *log);

//extern process * kernel_proc ; 

void handle_guest_message(const char *message)
{
	fprintf(guestlog, "%s\n", message);
        switch (message[0]) {
        case 'P':
                parse_process(message);
                break;
        case 'M':
                parse_module(message);
                break;
        case 'F':
                parse_function(message);
                break;
        }
}


//FIXME: this function may potentially overflow "buf" --Heng
static inline int readustr_with_cr3(uint32_t addr, uint32_t cr3, void *buf,
    CPUState *_env) {
  uint32_t unicode_data[2];
  int i, j, unicode_len = 0;
  uint8_t unicode_str[MAX_UNICODE_LENGTH] = { '\0' };
  char *store = (char *) buf;

  if (DECAF_read_mem(_env, addr, sizeof(unicode_data), unicode_data) < 0) {
    store[0] = '\0';
    goto done;
  }

  unicode_len = (int) (unicode_data[0] & 0xFFFF);
  if (unicode_len > MAX_UNICODE_LENGTH)
    unicode_len = MAX_UNICODE_LENGTH;

  if (DECAF_read_mem(_env, unicode_data[1], unicode_len, (void *) unicode_str) < 0) {
    store[0] = '\0';
    goto done;
  }

  for (i = 0, j = 0; i < unicode_len; i += 2, j++) {
    store[j] = tolower(unicode_str[i]);
  }
  store[j] = '\0';

done:
  return j;
}
int is_fullname_retrieved(process *proc)
{
  module * m = NULL;
  unordered_map<uint32_t, module *>::iterator iter;
  if(proc == NULL)
    return 1;


    uint32_t index = 0;
    for (iter = proc->module_list.begin(); iter != proc->module_list.end();
        iter++) {
       m = iter->second;
       
       if(strlen(m->fullname) == 0)
       {
         
         //monitor_printf(default_mon, "upating mod len");

        return -1;
       }
    }

    return 1;

}

int update_kernel_modules(CPUState * _env, gva_t vaddr)
{
  return 0;


}

int procmod_insert_modinfo(uint32_t pid, uint32_t cr3, const char *name,
			   uint32_t base, uint32_t size, const char *full_name)
{
  assert(strlen(name) < VMI_MAX_MODULE_PROCESS_NAME_LEN);
  assert(strlen(full_name) < VMI_MAX_MODULE_FULL_NAME_LEN);
  unordered_map<uint32_t, process *>::iterator iter = process_pid_map.find(
      pid);
  process *proc;

  if (iter == process_pid_map.end()) //pid not found
    return -1;

  proc = iter->second;

  if(strlen(proc->name) ==0)
  {
   


   // monitor_printf(default_mon, "insert modinfo \n");
    strcpy(proc->name, name);
    proc->cr3 = cr3;
   process_map[proc->cr3] = proc;

   VMI_dipatch_lmm(proc);

  }


  module *m = new module();
  m->size = size;
  strcpy(m->name, name);
  strcpy(m->fullname, full_name);
  //monitor_printf(default_mon, "name %s Fullname %s\n", name, full_name);
  VMI_insert_module(pid, base, m);
  return 0;
}

int procmod_remove_modinfo(uint32_t pid, uint32_t base)
{
   
   VMI_remove_module(pid, base);
    return 0;
}


int procmod_createproc(uint32_t pid, uint32_t parent_pid,
           uint32_t cr3, const char *name)
{



  process *proc = new process();
//TODO:FIXME incompleted process info
  proc->pid = pid;
  proc->parent_pid = parent_pid;
  proc->cr3 = cr3;
  strcpy(proc->name, name);

    // unordered_map < uint32_t, process * >::iterator iter =
    //   process_pid_map.find(pid);
    // if (iter != process_pid_map.end()){
    //   // Found an existing process with the same pid
    //   // We force to remove that one.
    // //  monitor_printf(default_mon, "remove process pid %d", proc->pid);
    //   VMI_remove_process(pid);
    // }

    // unordered_map < uint32_t, process * >::iterator iter2 =
    //       process_map.find(proc->cr3);
    // if (iter2 != process_map.end()) {
    //   // Found an existing process with the same CR3
    //   // We force to remove that process
    // //  monitor_printf(default_mon, "removing due to cr3 0x%08x\n", proc->cr3);
    //   if(proc->cr3 != -1)
    //     VMI_remove_process(iter2->second->pid);
    // }

    // process_pid_map[proc->pid] = proc;
    // process_map[proc->cr3] = proc;
    VMI_create_process(proc);

  return 0;
}

int procmod_removeproc(uint32_t pid)
{
	VMI_remove_process(pid);
	return 0;
}

int procmod_update_name(uint32_t pid, char *name)
{
	
  VMI_update_name( pid, name);
	return 0;
}


static int procmod_remove_all()
{
    VMI_remove_all();
    return 0;
}


// void update_proc(void *opaque)
// {
 
//   monitor_printf(default_mon, "updating proc \n");
// //    long taskaddr = 0xC033C300; 
//     int pid;
//     uint32_t cr3, pgd, mmap;
//     uint32_t nextaddr = 0;

//     char comm[512];

//     procmod_remove_all();

//     nextaddr = taskaddr;
//     do {
// 	pid = get_pid(nextaddr);
// 	pgd = get_pgd(nextaddr);
// 	cr3 = pgd - 0xc0000000;	//subtract a page offset 
// 	get_name(nextaddr, comm, 512);
// 	procmod_createproc(pid, -1, cr3, comm);	//XXX: parent pid is not supported

// 	mmap = get_first_mmap(nextaddr);
// 	while (0 != mmap) {
// 	    get_mod_name(mmap, comm, 512);
// 	    //term_printf("0x%08lX -- 0x%08lX %s\n", get_vmstart(env, mmap),
// 	    //            get_vmend(env, mmap), comm); 
// 	    int base = get_vmstart(mmap);
// 	    int size = get_vmend(mmap) - get_vmstart(mmap);
// 	 //  TODO: FIXME

//      // procmod_insert_modinfo(pid, pgd, comm, base, size);

// 	    char message[612];
// 	    snprintf(message, sizeof(message), "M %d %x \"%s\" %x %d", pid,
// 		     pgd, comm, base, size);
// 	    handle_guest_message(message);

// 	    char funcfile[128];
// 	    snprintf(funcfile, 128, "/tmp/%s.func", comm);
// 	    FILE *fp = fopen(funcfile, "r");
// 	    if (fp) {
// 		while (!feof(fp)) {
// 		    int offset;
// 		    char fname[128];
// 		    if (fscanf(fp, "%x %128s", &offset, fname) == 2) {
// 			snprintf(message, 128, "F %s %s %x ", comm,
// 				 fname, offset);
// 			handle_guest_message(message);
// 		    }
// 		}
// 		fclose(fp);
// 	    }

// 	    mmap = get_next_mmap(mmap);
// 	}

// 	nextaddr = next_task_struct(nextaddr);

//     } while (nextaddr != taskaddr);

// }

int procmod_init()
{
  
  process *kernel_proc = new process();
  kernel_proc->cr3 = 0;
  strcpy(kernel_proc->name, "<kernel>");
  kernel_proc->pid = 0;
  VMI_create_process(kernel_proc);

  
/*
  FILE *guestlog = fopen("guest.log", "r");
  char syslogline[GUEST_MESSAGE_LEN];
  int pos = 0;
  if (guestlog) {
    int ch;
    while ((ch = fgetc(guestlog)) != EOF) {
      syslogline[pos++] = (char) ch;
      if (pos > GUEST_MESSAGE_LEN - 2)
        pos = GUEST_MESSAGE_LEN - 2;
      if (ch == 0xa) {
        syslogline[pos] = 0;
        handle_guest_message(syslogline);
        pos = 0;
      }
    }
    fclose(guestlog);
  } */

    //TODO: save and load thread information
 //   monitor_printf(default_mon, " hookingpoint 0x%08x \n", hookingpoint);
    // if (init_kernel_offsets() >= 0)
    //  hookapi_hook_function(1, hookingpoint, 0, update_proc, NULL, 0);
 //   monitor_printf(default_mon, " hookingpoint 0x%08x \n", hookingpoint);
    // AWH - Added NULL parm for DeviceState* (change in API)
  //TODO: FIXME
  //  register_savevm(NULL, "procmod", 0, 1, procmod_save, procmod_load, NULL);
    return 0;
}

#define BOUNDED_STR(len) "%" #len "s"
#define BOUNDED_QUOTED(len) "%" #len "[^\"]"
#define BOUNDED_STR_x(len) BOUNDED_STR(len)
#define BOUNDED_QUOTED_x(len) BOUNDED_QUOTED(len)
#define BSTR BOUNDED_STR_x(GUEST_MESSAGE_LEN_MINUS_ONE)
#define BQUOT BOUNDED_QUOTED_x(GUEST_MESSAGE_LEN_MINUS_ONE)

void parse_process(const char *log)
{
	char c;
	uint32_t pid;
	uint32_t parent_pid = -1;
	uint32_t cr3;
	char name[16]="";


	if (sscanf(log, "P %c %d %d %08x %s", &c, &pid, &parent_pid, &cr3, name) < 2) {
		return;
	}
	switch (c) {
	case '-':
		procmod_removeproc(pid);
		break;
	case '+':
		if(strlen(name))
			procmod_createproc(pid, parent_pid, cr3, name);
		else
			procmod_createproc(pid, parent_pid, -1, "");
		break;
	case '^':
		procmod_update_name(pid, name);
		break;
	default:

		abort();
	}
}


void parse_module(const char *log)
{
 
  uint32_t pid, cr3, base, size;
  char mod[GUEST_MESSAGE_LEN];
  char full_mod[GUEST_MESSAGE_LEN]="";
  char c = '+';
  //We try to parse a long name with quotations first. If failed, we parse in the old way,
  //for backward compatibility. -Heng

  if (sscanf(log, "M %d %x \"" BQUOT "\" %x %x \"" BQUOT "\" %c", &pid, &cr3, mod, &base,
                                  &size, full_mod, &c) < 5 &&
      sscanf(log, "M %d %x \"" BSTR "\" %x %x \"" BQUOT "\" %c", &pid, &cr3, mod, &base,
                   &size, full_mod, &c) <5 )
    //no valid format is found
    return;
  
  switch (c) {
  case '-':
          procmod_remove_modinfo(pid, base);
          break;
  case '+':
          procmod_insert_modinfo(pid, cr3, mod, base, size, full_mod);
          break;
  default:
      assert(false);
  }  
}


int is_guest_windows()
{
#ifdef TARGET_I386
    //FIXME: we use a very simple hack here. Windows uses FS segment register to store 
    // the current process context, while Linux does not. We may need better heuristics 
    // when we need to support more guest systems.
    return (cpu_single_env->segs[R_FS].selector != 0);
#else
    return 0;
#endif
}


