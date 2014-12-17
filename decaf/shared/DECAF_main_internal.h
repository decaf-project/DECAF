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
#ifndef _DECAF_MAIN_INTERNAL_H_
#define _DECAF_MAIN_INTERNAL_H_

#include "monitor.h"

//LOK: Separate data structure for DECAF commands and plugin commands
extern mon_cmd_t DECAF_mon_cmds[];
extern mon_cmd_t DECAF_info_cmds[];

/****** Functions used internally ******/
extern void DECAF_nic_receive(const uint8_t * buf, const int size, const int cur_pos, const int start, const int stop);
extern void DECAF_nic_send(const uint32_t addr, const int size, const uint8_t * buf);
extern void DECAF_nic_in(const uint32_t addr, const int size);
extern void DECAF_nic_out(const uint32_t addr, const int size);
extern void DECAF_read_keystroke(void *s);
extern void DECAF_virtdev_init(void);
extern void DECAF_after_loadvm(const char *); // AWH void);
extern void DECAF_init(void);

extern void DECAF_update_cpl(int cpl);
//extern void DECAF_do_interrupt(int intno, int is_int, target_ulong next_eip);
extern void DECAF_after_iret_protected(void);
//extern void TEMU_update_cpustate(void);
extern void DECAF_loadvm(void *opaque);

#endif //_DECAF_MAIN_INTERNAL_H_
