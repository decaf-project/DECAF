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
#ifndef TAINT_INIT_H_
#define TAINT_INIT_H_
#include <xed-interface.h>
//#include "shared/read_linux.h"
//#include "../plugins/tracecap/trace.h"
#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif


#define stringify( name ) # name

int taintPrint_flag = 0;
int eaxTaint = 0;
void (*cb[256])() = {};

/* Map from XED register numbers to
    0) Christopher's register numbers
    1) Regnum
    2) Registry size in bytes
*/
int xed2chris_regmapping[][3] = {
/* XED_REG_INVALID */ {-1, -1, -1},
/* XED_REG_CR0 */ {-1, -1, -1},
/* XED_REG_CR1 */ {-1, -1, -1},
/* XED_REG_CR2 */ {-1, -1, -1},
/* XED_REG_CR3 */ {-1, -1, -1},
/* XED_REG_CR4 */ {-1, -1, -1},
/* XED_REG_CR5 */ {-1, -1, -1},
/* XED_REG_CR6 */ {-1, -1, -1},
/* XED_REG_CR7 */ {-1, -1, -1},
/* XED_REG_CR8 */ {-1, -1, -1},
/* XED_REG_CR9 */ {-1, -1, -1},
/* XED_REG_CR10 */ {-1, -1, -1},
/* XED_REG_CR11 */ {-1, -1, -1},
/* XED_REG_CR12 */ {-1, -1, -1},
/* XED_REG_CR13 */ {-1, -1, -1},
/* XED_REG_CR14 */ {-1, -1, -1},
/* XED_REG_CR15 */ {-1, -1, -1},
/* XED_REG_DR0 */ {-1, -1, -1},
/* XED_REG_DR1 */ {-1, -1, -1},
/* XED_REG_DR2 */ {-1, -1, -1},
/* XED_REG_DR3 */ {-1, -1, -1},
/* XED_REG_DR4 */ {-1, -1, -1},
/* XED_REG_DR5 */ {-1, -1, -1},
/* XED_REG_DR6 */ {-1, -1, -1},
/* XED_REG_DR7 */ {-1, -1, -1},
/* XED_REG_DR8 */ {-1, -1, -1},
/* XED_REG_DR9 */ {-1, -1, -1},
/* XED_REG_DR10 */ {-1, -1, -1},
/* XED_REG_DR11 */ {-1, -1, -1},
/* XED_REG_DR12 */ {-1, -1, -1},
/* XED_REG_DR13 */ {-1, -1, -1},
/* XED_REG_DR14 */ {-1, -1, -1},
/* XED_REG_DR15 */ {-1, -1, -1},
/* XED_REG_FLAGS */ {-1, -1, -1},
// Change this line to introduce eflags as an operand
/* XED_REG_EFLAGS */ {-1, -1, -1}, //{eflags_reg, R_EFLAGS},
/* XED_REG_RFLAGS */ {-1, -1, -1},
/* XED_REG_AX */ {ax_reg, R_EAX, 2},
/* XED_REG_CX */ {cx_reg, R_ECX, 2},
/* XED_REG_DX */ {dx_reg, R_EDX, 2},
/* XED_REG_BX */ {bx_reg, R_EBX, 2},
/* XED_REG_SP */ {sp_reg, R_ESP, 2},
/* XED_REG_BP */ {bp_reg, R_EBP, 2},
/* XED_REG_SI */ {si_reg, R_ESI, 2},
/* XED_REG_DI */ {di_reg, R_EDI, 2},
/* XED_REG_R8W */ {-1, -1, -1},
/* XED_REG_R9W */ {-1, -1, -1},
/* XED_REG_R10W */ {-1, -1, -1},
/* XED_REG_R11W */ {-1, -1, -1},
/* XED_REG_R12W */ {-1, -1, -1},
/* XED_REG_R13W */ {-1, -1, -1},
/* XED_REG_R14W */ {-1, -1, -1},
/* XED_REG_R15W */ {-1, -1, -1},
/* XED_REG_EAX */ {eax_reg, R_EAX, 4},
/* XED_REG_ECX */ {ecx_reg, R_ECX, 4},
/* XED_REG_EDX */ {edx_reg, R_EDX, 4},
/* XED_REG_EBX */ {ebx_reg, R_EBX, 4},
/* XED_REG_ESP */ {esp_reg, R_ESP, 4},
/* XED_REG_EBP */ {ebp_reg, R_EBP, 4},
/* XED_REG_ESI */ {esi_reg, R_ESI, 4},
/* XED_REG_EDI */ {edi_reg, R_EDI, 4},
/* XED_REG_R8D */ {-1, -1, -1},
/* XED_REG_R9D */ {-1, -1, -1},
/* XED_REG_R10D */ {-1, -1, -1},
/* XED_REG_R11D */ {-1, -1, -1},
/* XED_REG_R12D */ {-1, -1, -1},
/* XED_REG_R13D */ {-1, -1, -1},
/* XED_REG_R14D */ {-1, -1, -1},
/* XED_REG_R15D */ {-1, -1, -1},
/* XED_REG_RAX */ {-1, -1, -1},
/* XED_REG_RCX */ {-1, -1, -1},
/* XED_REG_RDX */ {-1, -1, -1},
/* XED_REG_RBX */ {-1, -1, -1},
/* XED_REG_RSP */ {-1, -1, -1},
/* XED_REG_RBP */ {-1, -1, -1},
/* XED_REG_RSI */ {-1, -1, -1},
/* XED_REG_RDI */ {-1, -1, -1},
/* XED_REG_R8 */ {-1, -1, -1},
/* XED_REG_R9 */ {-1, -1, -1},
/* XED_REG_R10 */ {-1, -1, -1},
/* XED_REG_R11 */ {-1, -1, -1},
/* XED_REG_R12 */ {-1, -1, -1},
/* XED_REG_R13 */ {-1, -1, -1},
/* XED_REG_R14 */ {-1, -1, -1},
/* XED_REG_R15 */ {-1, -1, -1},
/* XED_REG_AL */ {al_reg, R_EAX, 1},
/* XED_REG_CL */ {cl_reg, R_ECX, 1},
/* XED_REG_DL */ {dl_reg, R_EDX, 1},
/* XED_REG_BL */ {bl_reg, R_EBX, 1},
/* XED_REG_SPL */ {-1, -1, -1},
/* XED_REG_BPL */ {-1, -1, -1},
/* XED_REG_SIL */ {-1, -1, -1},
/* XED_REG_DIL */ {-1, -1, -1},
/* XED_REG_R8B */ {-1, -1, -1},
/* XED_REG_R9B */ {-1, -1, -1},
/* XED_REG_R10B */ {-1, -1, -1},
/* XED_REG_R11B */ {-1, -1, -1},
/* XED_REG_R12B */ {-1, -1, -1},
/* XED_REG_R13B */ {-1, -1, -1},
/* XED_REG_R14B */ {-1, -1, -1},
/* XED_REG_R15B */ {-1, -1, -1},
/* XED_REG_AH */ {ah_reg, R_EAX, 1},
/* XED_REG_CH */ {ch_reg, R_ECX, 1},
/* XED_REG_DH */ {dh_reg, R_EDX, 1},
/* XED_REG_BH */ {bh_reg, R_EBX, 1},
/* XED_REG_ERROR */ {-1, -1, -1},
/* XED_REG_RIP */ {-1, -1, -1},
/* XED_REG_EIP */ {-1, -1, -1},
/* XED_REG_IP */ {-1, -1, -1},
/* XED_REG_MMX0 */ {mmx0_reg, R_MMX0, 8},
/* XED_REG_MMX1 */ {mmx1_reg, R_MMX1, 8},
/* XED_REG_MMX2 */ {mmx2_reg, R_MMX2, 8},
/* XED_REG_MMX3 */ {mmx3_reg, R_MMX3, 8},
/* XED_REG_MMX4 */ {mmx4_reg, R_MMX4, 8},
/* XED_REG_MMX5 */ {mmx5_reg, R_MMX5, 8},
/* XED_REG_MMX6 */ {mmx6_reg, R_MMX6, 8},
/* XED_REG_MMX7 */ {mmx7_reg, R_MMX7, 8},
/* XED_REG_MXCSR */ {-1, -1, -1},
/* XED_REG_STACKPUSH */ {-1, -1, -1},
/* XED_REG_STACKPOP */ {-1, -1, -1},
/* XED_REG_GDTR */ {-1, -1, -1},
/* XED_REG_LDTR */ {-1, -1, -1},
/* XED_REG_IDTR */ {-1, -1, -1},
/* XED_REG_TR */ {-1, -1, -1},
/* XED_REG_TSC */ {-1, -1, -1},
/* XED_REG_TSCAUX */ {-1, -1, -1},
/* XED_REG_MSRS */ {-1, -1, -1},
/* XED_REG_X87CONTROL */ {fpu_control_reg, fpu_control_reg, 2},
/* XED_REG_X87STATUS */ {fpu_status_reg, fpu_status_reg, 2},
/* XED_REG_X87TOP */ {-1, -1, -1},
/* XED_REG_X87TAG */ {-1, -1, -1}, //{fpu_tag_reg, fpu_tag_reg, 2},
/* XED_REG_X87PUSH */ {-1, -1, -1},
/* XED_REG_X87POP */ {-1, -1, -1},
/* XED_REG_X87POP2 */ {-1, -1, -1},
/* XED_REG_CS */ {cs_reg, R_CS, 2},
/* XED_REG_DS */ {ds_reg, R_DS, 2},
/* XED_REG_ES */ {es_reg, R_ES, 2},
/* XED_REG_SS */ {ss_reg, R_SS, 2},
/* XED_REG_FS */ {fs_reg, R_FS, 2},
/* XED_REG_GS */ {gs_reg, R_GS, 2},
/* XED_REG_TMP0 */ {-1, -1, -1},
/* XED_REG_TMP1 */ {-1, -1, -1},
/* XED_REG_TMP2 */ {-1, -1, -1},
/* XED_REG_TMP3 */ {-1, -1, -1},
/* XED_REG_TMP4 */ {-1, -1, -1},
/* XED_REG_TMP5 */ {-1, -1, -1},
/* XED_REG_TMP6 */ {-1, -1, -1},
/* XED_REG_TMP7 */ {-1, -1, -1},
/* XED_REG_TMP8 */ {-1, -1, -1},
/* XED_REG_TMP9 */ {-1, -1, -1},
/* XED_REG_TMP10 */ {-1, -1, -1},
/* XED_REG_TMP11 */ {-1, -1, -1},
/* XED_REG_TMP12 */ {-1, -1, -1},
/* XED_REG_TMP13 */ {-1, -1, -1},
/* XED_REG_TMP14 */ {-1, -1, -1},
/* XED_REG_TMP15 */ {-1, -1, -1},
/* XED_REG_ST0 */ {fpu_st0_reg, R_ST0, 10},
/* XED_REG_ST1 */ {fpu_st1_reg, R_ST1, 10},
/* XED_REG_ST2 */ {fpu_st2_reg, R_ST2, 10},
/* XED_REG_ST3 */ {fpu_st3_reg, R_ST3, 10},
/* XED_REG_ST4 */ {fpu_st4_reg, R_ST4, 10},
/* XED_REG_ST5 */ {fpu_st5_reg, R_ST5, 10},
/* XED_REG_ST6 */ {fpu_st6_reg, R_ST6, 10},
/* XED_REG_ST7 */ {fpu_st7_reg, R_ST7, 10},
/* XED_REG_XMM0 */ {xmm0_reg, R_XMM0, 16},
/* XED_REG_XMM1 */ {xmm1_reg, R_XMM1, 16},
/* XED_REG_XMM2 */ {xmm2_reg, R_XMM2, 16},
/* XED_REG_XMM3 */ {xmm3_reg, R_XMM3, 16},
/* XED_REG_XMM4 */ {xmm4_reg, R_XMM4, 16},
/* XED_REG_XMM5 */ {xmm5_reg, R_XMM5, 16},
/* XED_REG_XMM6 */ {xmm6_reg, R_XMM6, 16},
/* XED_REG_XMM7 */ {xmm7_reg, R_XMM7, 16},
/* XED_REG_XMM8 */ {xmm8_reg, R_XMM8, 16},
/* XED_REG_XMM9 */ {xmm9_reg, R_XMM9, 16},
/* XED_REG_XMM10 */ {xmm10_reg, R_XMM10, 16},
/* XED_REG_XMM11 */ {xmm11_reg, R_XMM11, 16},
/* XED_REG_XMM12 */ {xmm12_reg, R_XMM12, 16},
/* XED_REG_XMM13 */ {xmm13_reg, R_XMM13, 16},
/* XED_REG_XMM14 */ {xmm14_reg, R_XMM14, 16},
/* XED_REG_XMM15 */ {xmm15_reg, R_XMM15, 16},
/* XED_REG_YMM0 */ {-1, -1, -1},
/* XED_REG_YMM1 */ {-1, -1, -1},
/* XED_REG_YMM2 */ {-1, -1, -1},
/* XED_REG_YMM3 */ {-1, -1, -1},
/* XED_REG_YMM4 */ {-1, -1, -1},
/* XED_REG_YMM5 */ {-1, -1, -1},
/* XED_REG_YMM6 */ {-1, -1, -1},
/* XED_REG_YMM7 */ {-1, -1, -1},
/* XED_REG_YMM8 */ {-1, -1, -1},
/* XED_REG_YMM9 */ {-1, -1, -1},
/* XED_REG_YMM10 */ {-1, -1, -1},
/* XED_REG_YMM11 */ {-1, -1, -1},
/* XED_REG_YMM12 */ {-1, -1, -1},
/* XED_REG_YMM13 */ {-1, -1, -1},
/* XED_REG_YMM14 */ {-1, -1, -1},
/* XED_REG_YMM15 */ {-1, -1, -1},
/* XED_REG_LAST */ {-1, -1, -1},
/* XED_REG_CR_FIRST */ {-1, -1, -1},
/* XED_REG_CR_LAST */ {-1, -1, -1},
/* XED_REG_DR_FIRST */ {-1, -1, -1},
/* XED_REG_DR_LAST */ {-1, -1, -1},
/* XED_REG_FLAGS_FIRST */ {-1, -1, -1},
/* XED_REG_FLAGS_LAST */ {-1, -1, -1},
/* XED_REG_GPR16_FIRST */ {-1, -1, -1},
/* XED_REG_GPR16_LAST */ {-1, -1, -1},
/* XED_REG_GPR32_FIRST */ {-1, -1, -1},
/* XED_REG_GPR32_LAST */ {-1, -1, -1},
/* XED_REG_GPR64_FIRST */ {-1, -1, -1},
/* XED_REG_GPR64_LAST */ {-1, -1, -1},
/* XED_REG_GPR8_FIRST */ {-1, -1, -1},
/* XED_REG_GPR8_LAST */ {-1, -1, -1},
/* XED_REG_GPR8H_FIRST */ {-1, -1, -1},
/* XED_REG_GPR8H_LAST */ {-1, -1, -1},
/* XED_REG_INVALID_FIRST */ {-1, -1, -1},
/* XED_REG_INVALID_LAST */ {-1, -1, -1},
/* XED_REG_IP_FIRST */ {-1, -1, -1},
/* XED_REG_IP_LAST */ {-1, -1, -1},
/* XED_REG_MMX_FIRST */ {-1, -1, -1},
/* XED_REG_MMX_LAST */ {-1, -1, -1},
/* XED_REG_MXCSR_FIRST */ {-1, -1, -1},
/* XED_REG_MXCSR_LAST */ {-1, -1, -1},
/* XED_REG_PSEUDO_FIRST */ {-1, -1, -1},
/* XED_REG_PSEUDO_LAST */ {-1, -1, -1},
/* XED_REG_SR_FIRST */ {-1, -1, -1},
/* XED_REG_SR_LAST */ {-1, -1, -1},
/* XED_REG_TMP_FIRST */ {-1, -1, -1},
/* XED_REG_TMP_LAST */ {-1, -1, -1},
/* XED_REG_X87_FIRST */ {-1, -1, -1},
/* XED_REG_X87_LAST */ {-1, -1, -1},
/* XED_REG_XMM_FIRST */ {-1, -1, -1},
/* XED_REG_XMM_LAST */ {-1, -1, -1},
/* XED_REG_YMM_FIRST */ {-1, -1, -1},
/* XED_REG_YMM_LAST */ {-1, -1, -1},
};

const char* OpTypesNames[] =
{
		stringify(TNone),
		stringify(TRegister),
		stringify(TMemLoc),
		stringify(TImmediate),
		stringify(TJump),
		stringify(TFloatRegister)

};

const char* OpUsageNames[] =
{
		stringify(unknown),
		stringify(esp),
		stringify(counter),
		stringify(membase),
		stringify(memindex),
		stringify(memsegment),
		stringify(memsegent0),
		stringify(memsegent1),
		stringify(memdisplacement),
		stringify(memscale),
		stringify(eflags)
};

void decode_address(uint32_t address, EntryHeader *eh, int ignore_taint);
int taintInit(unsigned long);
void xed2_init(void);
void taint_add_handler(uint32_t eip);
void taint_addr(int addr);
void MOV_Ev_Gv(void);
void MOV_Eb_Gb(uint32_t,uint32_t);

#ifdef __cplusplus
}
#endif
#endif
