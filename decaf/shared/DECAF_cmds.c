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
#include "DECAF_main.h"
#include "DECAF_cmds.h"
#include "vmi_c_wrapper.h"

void do_guest_ps(Monitor *mon)
{
 VMI_list_processes(mon);
}

void do_print_modules(Monitor *mon)
{
	print_loaded_modules(cpu_single_env);
}



void do_guest_modules(Monitor *mon, const QDict *qdict)
{
  int pid = -1;

  //LOK: This check should be unnecessary since the
  // monitor should have taken care of it. However we leave it here
  /*
  if (qdict_haskey(qdict, "pid"))
  {
    pid = qdict_get_int(qdict, "pid");
  }    
 
  if (pid == -1)
  {
    monitor_printf(mon, "need a pid\n");
  }
  */
 	pid = qdict_get_int(qdict, "pid");
	VMI_list_modules(mon, pid);
}

extern int DECAF_kvm_enabled;

void do_toggle_kvm(Monitor *mon, const QDict *qdict)
{
    int status = qdict_get_bool(qdict, "status");
    if(DECAF_kvm_enabled == status) {
    	monitor_printf(default_mon, "KVM has already been turned %s!\n", status? "on": "off");
    	return;
    }

    DECAF_stop_vm();
    DECAF_kvm_enabled = status;
    DECAF_start_vm();
    monitor_printf(default_mon, "KVM is now turned %s!\n", status? "on": "off");

}

