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


#ifndef _DECAF_MAIN_X86_H_INCLUDED_
#define _DECAF_MAIN_X86_H_INCLUDED_

#undef INLINE

#include "config.h"
#include <assert.h>

#include "qemu-common.h"
#include "cpu.h"
//#include "taintcheck.h"
#include "targphys.h"
#include "compiler.h"
#include "monitor.h"
// AWH #include "shared/disasm.h"
#include "shared/DECAF_callback.h"

#define MAX_REGS (CPU_NUM_REGS + 8) //we assume up to 8 temporary registers

#ifdef __cplusplus // AWH
extern "C" {
#endif // __cplusplus

//extern void DECAF_update_cpustate(void);

/** Define Registers ***/
/* Most are defined in shared/disasm.h */

/* special registers */
#define eip_reg 140
#define cr3_reg 141
#define t0_reg  142
#define t1_reg  143
#define a0_reg  144
#define eflags_reg 145
#define indir_dx_reg 150

/* MMX registers */
#define mmx0_reg 164
#define mmx1_reg 165
#define mmx2_reg 166
#define mmx3_reg 167
#define mmx4_reg 168
#define mmx5_reg 169
#define mmx6_reg 170
#define mmx7_reg 171

/* XMM registers */
#define xmm0_reg  172
#define xmm1_reg  173
#define xmm2_reg  174
#define xmm3_reg  175
#define xmm4_reg  176
#define xmm5_reg  177
#define xmm6_reg  178
#define xmm7_reg  179
#define xmm8_reg  180
#define xmm9_reg  181
#define xmm10_reg 182
#define xmm11_reg 183
#define xmm12_reg 184
#define xmm13_reg 185
#define xmm14_reg 186
#define xmm15_reg 187

/* float data registers */
#define fpu_st0_reg 188
#define fpu_st1_reg 189
#define fpu_st2_reg 190
#define fpu_st3_reg 191
#define fpu_st4_reg 192
#define fpu_st5_reg 193
#define fpu_st6_reg 194
#define fpu_st7_reg 195

/* float control registers */
#define fpu_control_reg 196
#define fpu_status_reg  197
#define fpu_tag_reg     198

#define R_MMX0 0
#define R_MMX1 1
#define R_MMX2 2
#define R_MMX3 3
#define R_MMX4 4
#define R_MMX5 5
#define R_MMX6 6
#define R_MMX7 7

#define R_XMM0 0
#define R_XMM1 1
#define R_XMM2 2
#define R_XMM3 3
#define R_XMM4 4
#define R_XMM5 5
#define R_XMM6 6
#define R_XMM7 7
#define R_XMM8 8
#define R_XMM9 9
#define R_XMM10 10
#define R_XMM11 11
#define R_XMM12 12
#define R_XMM13 13
#define R_XMM14 14
#define R_XMM15 15

#define R_ST0 0
#define R_ST1 1
#define R_ST2 2
#define R_ST3 3
#define R_ST4 4
#define R_ST5 5
#define R_ST6 6
#define R_ST7 7

// AWH - These are no longer in cpu.h, but we'll keep them to avoid
// changing the TEMU code too much
#define R_T0 CPU_NB_REGS
#define R_T1 (CPU_NB_REGS + 1)
#define R_A0 (CPU_NB_REGS + 2)
#define R_CC_SRC (CPU_NB_REGS + 3)
#define R_CC_DST (CPU_NB_REGS + 4)
#define R_EFLAGS (CPU_NB_REGS + 5)



#if 0
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

#endif




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

/// Check if the current execution of guest system is in kernel mode (i.e., ring-0)
static inline int DECAF_is_in_kernel(CPUState *_env)
{
  return ((_env->hflags & HF_CPL_MASK) == 0);
}
#if 0 // AWH
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
    case eax_reg: *(uint32_t*)buf = cpu_single_env->regs[R_EAX];
    	break;
    case ecx_reg: *(uint32_t*)buf = cpu_single_env->regs[R_ECX];
    	break;
    case edx_reg: *(uint32_t*)buf = cpu_single_env->regs[R_EDX];
    	break;
    case ebx_reg: *(uint32_t*)buf = cpu_single_env->regs[R_EBX];
    	break;
    case esp_reg: *(uint32_t*)buf = cpu_single_env->regs[R_ESP];
    	break;
    case ebp_reg: *(uint32_t*)buf = cpu_single_env->regs[R_EBP];
    	break;
    case esi_reg: *(uint32_t*)buf = cpu_single_env->regs[R_ESI];
    	break;
    case edi_reg: *(uint32_t*)buf = cpu_single_env->regs[R_EDI];
    	break;

    case al_reg: *(uint8_t*)buf = (uint8_t)cpu_single_env->regs[R_EAX];
			   break;
    case cl_reg: *(uint8_t*)buf = (uint8_t)cpu_single_env->regs[R_ECX];
			   break;
    case dl_reg: *(uint8_t*)buf = (uint8_t)cpu_single_env->regs[R_EDX];
			   break;
    case bl_reg: *(uint8_t*)buf = (uint8_t)cpu_single_env->regs[R_EBX];
			   break;
    case ah_reg: *(uint8_t*)buf = *((uint8_t*)(&cpu_single_env->regs[R_EAX]) + 1);
			   break;
    case ch_reg: *(uint8_t*)buf = *((uint8_t*)(&cpu_single_env->regs[R_ECX]) + 1);
			   break;
    case dh_reg: *(uint8_t*)buf = *((uint8_t*)(&cpu_single_env->regs[R_EDX]) + 1);
			   break;
    case bh_reg: *(uint8_t*)buf = *((uint8_t*)(&cpu_single_env->regs[R_EBX]) + 1);
			   break;
       
    case ax_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_EAX];
			   break;
    case cx_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_ECX];
			   break;
    case dx_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_EDX];
			   break;
    case bx_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_EBX];
			   break;
    case sp_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_ESP];
			   break;
    case bp_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_EBP];
			   break;
    case si_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_ESI];
			   break;
    case di_reg: *(uint16_t*)buf = (uint16_t)cpu_single_env->regs[R_EDI];
			   break;

    case eip_reg: 
    		  *(uint32_t *)buf = cpu_single_env->eip;
    		break;
    case cr3_reg: 
    		*(uint32_t *)buf = cpu_single_env->cr[3];
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
    case eax_reg: cpu_single_env->regs[R_EAX] = *(uint32_t*)buf;
    	break;
    case ecx_reg: cpu_single_env->regs[R_ECX] = *(uint32_t*)buf;
    	break;
    case edx_reg: cpu_single_env->regs[R_EDX] = *(uint32_t*)buf;
    	break;
    case ebx_reg: cpu_single_env->regs[R_EBX] = *(uint32_t*)buf;
    	break;
    case esp_reg: cpu_single_env->regs[R_ESP] = *(uint32_t*)buf;
    	break;
    case ebp_reg: cpu_single_env->regs[R_EBP] = *(uint32_t*)buf;
    	break;
    case esi_reg: cpu_single_env->regs[R_ESI] = *(uint32_t*)buf;
    	break;
    case edi_reg: cpu_single_env->regs[R_EDI] = *(uint32_t*)buf;
    	break;

    case al_reg: *(uint8_t*)(&cpu_single_env->regs[R_EAX]) = *(uint8_t*)buf;
			   break;
    case cl_reg: *(uint8_t*)(&cpu_single_env->regs[R_ECX]) = *(uint8_t*)buf;
			   break;
    case dl_reg: *(uint8_t*)(&cpu_single_env->regs[R_EDX]) = *(uint8_t*)buf;
			   break;
    case bl_reg: *(uint8_t*)(&cpu_single_env->regs[R_EBX]) = *(uint8_t*)buf;
			   break;
    case ah_reg: *((uint8_t*)(&cpu_single_env->regs[R_EAX]) + 1) = *(uint8_t*)buf;
			   break;
    case ch_reg: *((uint8_t*)(&cpu_single_env->regs[R_ECX]) + 1) = *(uint8_t*)buf;
			   break;
    case dh_reg: *((uint8_t*)(&cpu_single_env->regs[R_EDX]) + 1) = *(uint8_t*)buf;
			   break;
    case bh_reg: *((uint8_t*)(&cpu_single_env->regs[R_EBX]) + 1) = *(uint8_t*)buf;
			   break;
       
    case ax_reg: *(uint16_t*)(&cpu_single_env->regs[R_EAX]) = *(uint16_t*)buf;
			   break;
    case cx_reg: *(uint16_t*)(&cpu_single_env->regs[R_ECX]) = *(uint16_t*)buf;
			   break;
    case dx_reg: *(uint16_t*)(&cpu_single_env->regs[R_EDX]) = *(uint16_t*)buf;
			   break;
    case bx_reg: *(uint16_t*)(&cpu_single_env->regs[R_EBX]) = *(uint16_t*)buf;
			   break;
    case sp_reg: *(uint16_t*)(&cpu_single_env->regs[R_ESP]) = *(uint16_t*)buf;
			   break;
    case bp_reg: *(uint16_t*)(&cpu_single_env->regs[R_EBP]) = *(uint16_t*)buf;
			   break;
    case si_reg: *(uint16_t*)(&cpu_single_env->regs[R_ESI]) = *(uint16_t*)buf;
			   break;
    case di_reg: *(uint16_t*)(&cpu_single_env->regs[R_EDI]) = *(uint16_t*)buf;
			   break;

    case eip_reg: 
    		cpu_single_env->eip = *(uint32_t *)buf;
    		break;
    case cr3_reg: 
    		cpu_single_env->cr[3] = *(uint32_t *)buf;
    		break;

    default: 
     	assert(0);
  }
}
#endif // AWH
/* @} */ //end of group

static inline gva_t DECAF_getPC(CPUState* env)
{
  return (env->eip + env->segs[R_CS].base);
}

static inline gpa_t DECAF_getPGD(CPUState* env)
{
  return (env->cr[3]);
}

static inline gva_t DECAF_getESP(CPUState* env)
{
  return (env->regs[R_ESP]);
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif //_DECAF_MAIN_X86_H_INCLUDED_
