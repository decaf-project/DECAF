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
/********************************************************************
** function_map.h
** Author: Heng Yin <heyin@syr.edu>
**
*
** this module maps eips to function names.  this 
** facilitates printing the names of functions called
** by executables run inside TEMU.
**
*/

#ifndef _FUNCTION_MAP_H_
#define _FUNCTION_MAP_H_


#ifdef __cplusplus
extern "C" {
#endif

void function_map_init(void);
void function_map_cleanup(void);

target_ulong funcmap_get_pc(const char *module_name, const char *function_name, target_ulong cr3);

int funcmap_get_name_c(target_ulong pc, target_ulong cr3, char *mod_name, char *func_name);

void funcmap_insert_function(const char *module, const char *fname, uint32_t offset, uint32_t inode_number);

extern void parse_function(const char *message);

#ifdef __cplusplus
};
#endif


#endif

