/*
Copyright (C) <2013> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU LGPL, version 2.1 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/

/// @file DECAF_main_mips.h
/// @author: Heng Yin <hyin@ece.cmu.edu>
/// \addtogroup main temu: Main TEMU Module

// Edited by: Aravind Prakash <arprakas@syr.edu> : 2010-10-06
//This file is meant as an interface to the DECAF plugin. All other functions have been moved to DECAF_main_internal.h


#ifndef _DECAF_TARGET_H_INCLUDED_
#define _DECAF_TARGET_H_INCLUDED_

#undef INLINE

#include "config.h"
#include <assert.h>

#include "qemu-common.h"
#include "cpu.h"
//#include "taintcheck.h"
#include "targphys.h"
#include "compiler.h"
#include "monitor.h"
// #include "shared/disasm.h"
#include "shared/DECAF_callback.h"

#define MAX_REGS (CPU_NUM_REGS + 8) //we assume up to 8 temporary registers

#ifdef __cplusplus // AWH
extern "C" {
#endif // __cplusplus

/// Check if the current execution of guest system is in kernel mode (i.e., ring-0)
static inline int DECAF_is_in_kernel(CPUState *env)
{
  return ((env->hflags & MIPS_HFLAG_MODE) == MIPS_HFLAG_KM);
}


static inline gva_t DECAF_getPC(CPUState* env)
{
  return (env->active_tc.PC);
}

gpa_t DECAF_getPGD(CPUState* env); 

//Based this off of op_helper.c in r4k_fill_tlb()
// static inline gpa_t DECAF_getPGD(CPUState* env)
// {
//   return (env->CP0_EntryHi);
// }

static inline gva_t DECAF_getESP(CPUState* env)
{
  /* AWH - General-purpose register 29 (of 32) is the stack pointer */
  return (env->active_tc.gpr[29]);
}

/* AWH - In shared/DECAF_main.h */
//gpa_t DECAF_get_phys_addr_with_pgd(CPUState* env, gpa_t pgd, gva_t addr);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif //_DECAF_TARGET_H_INCLUDED_
