/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

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

/// @file DECAF_main_x86.h
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
  return (env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR;
}


static inline gva_t DECAF_getPC(CPUState* env)
{
  return (env->regs[15]);
}

//Based this off of helper.c in get_level1_table_address
static inline gpa_t DECAF_getPGD(CPUState* env)
{
  return (env->cp15.c2_base0 & env->cp15.c2_base_mask);
}

static inline gva_t DECAF_getESP(CPUState* env)
{
  return (env->regs[13]);
}

#if 0 // AWH - Comment in as needed

extern void DECAF_update_cpustate(void);

/*
 * These are extracted from CPUX86State
 */
/// array of CPU general-purpose registers, such as R_EAX, R_EBX
extern target_ulong *DECAF_cpu_regs;
/// pointer to instruction pointer EIP
extern target_ulong *DECAF_cpu_eip;
/// pointer to EFLAGS
extern target_ulong *DECAF_cpu_eflags;
/// pointer to hidden flags
extern uint32_t *DECAF_cpu_hflags;
/// array of CPU segment registers, such as R_FS, R_CS
extern SegmentCache *DECAF_cpu_segs;
/// pointer to LDT
extern SegmentCache *DECAF_cpu_ldt;
/// pointer to GDT
extern SegmentCache *DECAF_cpu_gdt;
/// pointer to IDT
extern SegmentCache *DECAF_cpu_idt;
/// array of CPU control registers, such as CR0 and CR1
extern target_ulong *DECAF_cpu_cr;
/// pointer to DF register
extern int32_t *DECAF_cpu_df;
/// array of XMM registers
extern XMMReg *DECAF_cpu_xmm_regs;
/// array of MMX registers
extern MMXReg *DECAF_cpu_mmx_regs;
/// FPU - array of Floating Point registers
extern FPReg *DECAF_cpu_fp_regs;
/// FPU - top of Floating Point register stack 
extern unsigned int * DECAF_cpu_fp_stt;
/// FPU - Status Register
extern uint16_t * DECAF_cpu_fpus;
/// FPU - Control Register
extern uint16_t * DECAF_cpu_fpuc;
/// FPU - Tag Register
extern uint8_t * DECAF_cpu_fptags;


extern uint32_t *DECAF_cc_op;






//Aravind - Function to register cb for range of insns.
extern void DECAF_register_insn_cb_range(uint32_t start_opcode, uint32_t end_opcode, void (*insn_cb_handler) (uint32_t, uint32_t));
//Function to cleanup insn_cbs. TODO: Should automatically happen when plugin exits.
//extern void DECAF_cleanup_insn_cbs(void);
//end - Aravind

/// \brief Given a virtual address, this function returns the page access status.
///
///  @param addr virtual memory address
///  @return page access status: -1 means not present, 0 means readonly,
///   and 1 means writable.
extern int DECAF_get_page_access(CPUState* env, gva_t addr);

/// \brief Read from a register.
///
/// Note that reg_id is register ID, which is different from register index.
/// Register ID is defined by Kruegel's disassembler, whereas register index is 
/// the index of CPU register array.
/// @param reg_id register ID
/// @param buf output buffer of the value to be read
static inline void DECAF_read_register(int reg_id, void *buf)
{
  switch(reg_id) {
    case eax_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_EAX];
    	break;
    case ecx_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_ECX];
    	break;
    case edx_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_EDX];
    	break;
    case ebx_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_EBX];
    	break;
    case esp_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_ESP];
    	break;
    case ebp_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_EBP];
    	break;
    case esi_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_ESI];
    	break;
    case edi_reg: *(uint32_t*)buf = DECAF_cpu_regs[R_EDI];
    	break;

    case al_reg: *(uint8_t*)buf = (uint8_t)DECAF_cpu_regs[R_EAX];
			   break;
    case cl_reg: *(uint8_t*)buf = (uint8_t)DECAF_cpu_regs[R_ECX];
			   break;
    case dl_reg: *(uint8_t*)buf = (uint8_t)DECAF_cpu_regs[R_EDX];
			   break;
    case bl_reg: *(uint8_t*)buf = (uint8_t)DECAF_cpu_regs[R_EBX];
			   break;
    case ah_reg: *(uint8_t*)buf = *((uint8_t*)(&DECAF_cpu_regs[R_EAX]) + 1);
			   break;
    case ch_reg: *(uint8_t*)buf = *((uint8_t*)(&DECAF_cpu_regs[R_ECX]) + 1);
			   break;
    case dh_reg: *(uint8_t*)buf = *((uint8_t*)(&DECAF_cpu_regs[R_EDX]) + 1);
			   break;
    case bh_reg: *(uint8_t*)buf = *((uint8_t*)(&DECAF_cpu_regs[R_EBX]) + 1);
			   break;
       
    case ax_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_EAX];
			   break;
    case cx_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_ECX];
			   break;
    case dx_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_EDX];
			   break;
    case bx_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_EBX];
			   break;
    case sp_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_ESP];
			   break;
    case bp_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_EBP];
			   break;
    case si_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_ESI];
			   break;
    case di_reg: *(uint16_t*)buf = (uint16_t)DECAF_cpu_regs[R_EDI];
			   break;

    case eip_reg: 
    		  *(uint32_t *)buf = *DECAF_cpu_eip;
    		break;
    case cr3_reg: 
    		*(uint32_t *)buf = DECAF_cpu_cr[3];
    		break;

    default: 
     	assert(0);
  }
}

/// \brief Write into a register.
///
/// Note that reg_id is register ID, which is different from register index.
/// Register ID is defined by Kruegel's disassembler, whereas register index is 
/// the index of CPU register array.
/// @param reg_id register ID
/// @param buf input buffer of the value to be written
static inline void DECAF_write_register(int reg_id, void *buf)
{

  switch(reg_id) {
    case eax_reg: DECAF_cpu_regs[R_EAX] = *(uint32_t*)buf;
    	break;
    case ecx_reg: DECAF_cpu_regs[R_ECX] = *(uint32_t*)buf;
    	break;
    case edx_reg: DECAF_cpu_regs[R_EDX] = *(uint32_t*)buf;
    	break;
    case ebx_reg: DECAF_cpu_regs[R_EBX] = *(uint32_t*)buf;
    	break;
    case esp_reg: DECAF_cpu_regs[R_ESP] = *(uint32_t*)buf;
    	break;
    case ebp_reg: DECAF_cpu_regs[R_EBP] = *(uint32_t*)buf;
    	break;
    case esi_reg: DECAF_cpu_regs[R_ESI] = *(uint32_t*)buf;
    	break;
    case edi_reg: DECAF_cpu_regs[R_EDI] = *(uint32_t*)buf;
    	break;

    case al_reg: *(uint8_t*)(&DECAF_cpu_regs[R_EAX]) = *(uint8_t*)buf;
			   break;
    case cl_reg: *(uint8_t*)(&DECAF_cpu_regs[R_ECX]) = *(uint8_t*)buf;
			   break;
    case dl_reg: *(uint8_t*)(&DECAF_cpu_regs[R_EDX]) = *(uint8_t*)buf;
			   break;
    case bl_reg: *(uint8_t*)(&DECAF_cpu_regs[R_EBX]) = *(uint8_t*)buf;
			   break;
    case ah_reg: *((uint8_t*)(&DECAF_cpu_regs[R_EAX]) + 1) = *(uint8_t*)buf;
			   break;
    case ch_reg: *((uint8_t*)(&DECAF_cpu_regs[R_ECX]) + 1) = *(uint8_t*)buf;
			   break;
    case dh_reg: *((uint8_t*)(&DECAF_cpu_regs[R_EDX]) + 1) = *(uint8_t*)buf;
			   break;
    case bh_reg: *((uint8_t*)(&DECAF_cpu_regs[R_EBX]) + 1) = *(uint8_t*)buf;
			   break;
       
    case ax_reg: *(uint16_t*)(&DECAF_cpu_regs[R_EAX]) = *(uint16_t*)buf;
			   break;
    case cx_reg: *(uint16_t*)(&DECAF_cpu_regs[R_ECX]) = *(uint16_t*)buf;
			   break;
    case dx_reg: *(uint16_t*)(&DECAF_cpu_regs[R_EDX]) = *(uint16_t*)buf;
			   break;
    case bx_reg: *(uint16_t*)(&DECAF_cpu_regs[R_EBX]) = *(uint16_t*)buf;
			   break;
    case sp_reg: *(uint16_t*)(&DECAF_cpu_regs[R_ESP]) = *(uint16_t*)buf;
			   break;
    case bp_reg: *(uint16_t*)(&DECAF_cpu_regs[R_EBP]) = *(uint16_t*)buf;
			   break;
    case si_reg: *(uint16_t*)(&DECAF_cpu_regs[R_ESI]) = *(uint16_t*)buf;
			   break;
    case di_reg: *(uint16_t*)(&DECAF_cpu_regs[R_EDI]) = *(uint16_t*)buf;
			   break;

    case eip_reg: 
    		*DECAF_cpu_eip = *(uint32_t *)buf;
    		break;
    case cr3_reg: 
    		DECAF_cpu_cr[3] = *(uint32_t *)buf;
    		break;

    default: 
     	assert(0);
  }
}
#endif // AWH
/* @} */ //end of group


#ifdef __cplusplus
}
#endif // __cplusplus

#endif //_DECAF_TARGET_H_INCLUDED_
