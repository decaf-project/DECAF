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
/**
 * @author Lok Yan
 * @date 14 October 2012
 *
 * DECAF_cmds.c and .h are used for implementing monitor commands default to DECAF.
 */

#ifndef DECAF_CMDS_H
#define DECAF_CMDS_H

#include "monitor.h"
#include "qdict.h"

#ifdef __cplusplus
extern "C"
{
#endif

//void do_linux_ps(Monitor *mon, const QDict* qdict);
void do_guest_ps(Monitor *mon);
void do_guest_modules(Monitor *mon, const QDict *qdict);
void do_toggle_kvm(Monitor *mon, const QDict *qdict);

void do_print_modules(Monitor *mon);
void print_loaded_modules(CPUState *env);



#ifdef __cplusplus
}
#endif

#endif//DECAF_CMDS_H
