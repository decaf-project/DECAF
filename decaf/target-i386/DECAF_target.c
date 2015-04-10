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
#include <assert.h>
#include <sys/queue.h>
#include "sysemu.h" // AWH
#include "qemu-timer.h" // AWH
#include "hw/hw.h"
#include "hw/isa.h"		/* for register_ioport_write */
#include "blockdev.h" // AWH
#include "shared/DECAF_main.h" // AWH
#include "shared/DECAF_callback.h"
#include "shared/hookapi.h" // AWH
#include "DECAF_target.h"
// Deprecated
int DECAF_emulation_started = 0;

#if 0
/*
 * A virtual device that reads messages from the kernel module in the guest windows
 */

target_ulong *DECAF_cpu_regs = NULL;
target_ulong *DECAF_cpu_eip = NULL;
target_ulong *DECAF_cpu_eflags = NULL;
uint32_t *DECAF_cpu_hflags = NULL;
SegmentCache *DECAF_cpu_segs = NULL;
SegmentCache *DECAF_cpu_ldt = NULL;
SegmentCache *DECAF_cpu_gdt = NULL;
SegmentCache *DECAF_cpu_idt = NULL;
target_ulong *DECAF_cpu_cr = NULL;
int32_t *DECAF_cpu_df = NULL;
uint32_t *DECAF_cc_op = NULL;
XMMReg *DECAF_cpu_xmm_regs = NULL;
MMXReg *DECAF_cpu_mmx_regs = NULL;
FPReg *DECAF_cpu_fp_regs = NULL;
unsigned int *DECAF_cpu_fp_stt = NULL;
uint16_t *DECAF_cpu_fpus = NULL;
uint16_t *DECAF_cpu_fpuc = NULL;
uint8_t *DECAF_cpu_fptags = NULL;
uint32_t *DECAF_cpu_branch_cnt = NULL;	//JMK: added by manju mjayacha@syr.edu to store the branch count value
int insn_tainted = 0;		//Indicates if the current instruction is tainted
int plugin_taint_record_size = 0;
#endif

// AWH extern CPUState *first_cpu;

#if 0
/*
 * Update CPU States 
 */
void DECAF_update_cpustate(void)
{
    static CPUState *last_env = 0;
    CPUState *cur_env = cpu_single_env ? cpu_single_env : first_cpu;
    if (last_env != cur_env) {
	last_env = cur_env;
	DECAF_cpu_regs = cur_env->regs;
	DECAF_cpu_eip = &cur_env->eip;
	DECAF_cpu_eflags = &cur_env->eflags;
	DECAF_cpu_hflags = &cur_env->hflags;
	DECAF_cpu_segs = cur_env->segs;
	DECAF_cpu_ldt = &cur_env->ldt;
	DECAF_cpu_gdt = &cur_env->gdt;
	DECAF_cpu_idt = &cur_env->idt;
	DECAF_cpu_cr = cur_env->cr;
	DECAF_cpu_df = &cur_env->df;
	DECAF_cc_op = &cur_env->cc_op;
	DECAF_cpu_xmm_regs = (XMMReg *) & cur_env->xmm_regs;
	//DECAF_cpu_mmx_regs = (MMXReg *)&cur_env->fpregs;
	DECAF_cpu_fp_regs = (FPReg *) (&(cur_env->fpregs));
	DECAF_cpu_fp_stt = &cur_env->fpstt;
	DECAF_cpu_fpus = &cur_env->fpus;
	DECAF_cpu_fpuc = &cur_env->fpuc;
	DECAF_cpu_fptags = cur_env->fptags;
	DECAF_cpu_branch_cnt = &cur_env->branch_cnt;
    }
}
#endif

#if 0 //TO BE REMOVED
void DECAF_update_cpl(int cpl)
{
    if (decaf_plugin) {
    	//TODO: may give the plugin a callback

#if 0 //LOK: Removed // AWH TAINT_ENABLED
	static int count = 1024 * 512;
	if (count-- <= 0) {
	    count = 1024 * 512;
	    garbage_collect();
	}
#endif
    }
}
#endif


gpa_t DECAF_get_phys_addr_with_pgd(CPUState* env, gpa_t pgd, gva_t addr)
{

  if (env == NULL)
  {
#ifdef DECAF_NO_FAIL_SAFE
    return (INV_ADDR);
#else
    env = cpu_single_env ? cpu_single_env : first_cpu;
#endif
  }

  target_ulong saved_cr3 = env->cr[3];
  uint32_t phys_addr;

  env->cr[3] = pgd;
  phys_addr = cpu_get_phys_page_debug(env, addr & TARGET_PAGE_MASK);

  env->cr[3] = saved_cr3;
  return (phys_addr | (addr & (~TARGET_PAGE_MASK)));
}



#if 0 //TO BE REMOVED -Heng
void DECAF_do_interrupt(int intno, int is_int, target_ulong next_eip)
{
#ifdef HANDLE_INTERRUPT
    if (decaf_plugin)
	decaf_plugin->do_interrupt(intno, is_int, next_eip);
#endif
}

void DECAF_after_iret_protected(void)
{
#ifdef HANDLE_INTERRUPT
    if (decaf_plugin && decaf_plugin->after_iret_protected)
	decaf_plugin->after_iret_protected();
#endif
}
#endif

int DECAF_get_page_access(CPUState* env, /*uint32_t*/target_ulong addr)
{
    uint32_t pde_addr, pte_addr;
    uint32_t pde, pte;

    if (env == NULL)
    {
  #ifdef DECAF_NO_FAIL_SAFE
      return (INV_ADDR);
  #else
      env = cpu_single_env ? cpu_single_env : first_cpu;
  #endif
    }

    if (env->cr[4] & CR4_PAE_MASK) {
	uint32_t pdpe_addr, pde_addr, pte_addr;
	uint32_t pdpe;

	pdpe_addr = ((env->cr[3] & ~0x1f) + ((addr >> 30) << 3)) &
	    env->a20_mask;
	pdpe = ldl_phys(pdpe_addr);
	if (!(pdpe & PG_PRESENT_MASK))
	    return -1;

	pde_addr = ((pdpe & ~0xfff) + (((addr >> 21) & 0x1ff) << 3)) &
	    env->a20_mask;
	pde = ldl_phys(pde_addr);
	if (!(pde & PG_PRESENT_MASK)) {
	    return -1;
	}
	if (pde & PG_PSE_MASK) {
	    /* 2 MB page */
	    return (pde & PG_RW_MASK);
	}

	/* 4 KB page */
	pte_addr = ((pde & ~0xfff) + (((addr >> 12) & 0x1ff) << 3)) &
	    env->a20_mask;
	pte = ldl_phys(pte_addr);
	if (!(pte & PG_PRESENT_MASK))
	    return -1;
	return (pte & PG_RW_MASK);
    }

    if (!(env->cr[0] & CR0_PG_MASK)) {
	/* page addressing is disabled */
	return 1;
    }

    /* page directory entry */
    pde_addr =
	((env->cr[3] & ~0xfff) + ((addr >> 20) & ~3)) & env->a20_mask;
    pde = ldl_phys(pde_addr);
    if (!(pde & PG_PRESENT_MASK))
	return -1;
    if ((pde & PG_PSE_MASK) && (env->cr[4] & CR4_PSE_MASK)) {
	/* page size is 4MB */
	return (pde & PG_RW_MASK);
    }

    /* page directory entry */
    pte_addr = ((pde & ~0xfff) + ((addr >> 10) & 0xffc)) & env->a20_mask;
    pte = ldl_phys(pte_addr);
    if (!(pte & PG_PRESENT_MASK))
	return -1;

    /* page size is 4K */
    return (pte & PG_RW_MASK);
}







