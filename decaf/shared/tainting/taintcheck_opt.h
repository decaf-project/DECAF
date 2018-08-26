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
//Removed --Heng
//uncomment this
#ifndef TAINTCHECK_OPT_H_INCLUDED
#define TAINTCHECK_OPT_H_INCLUDED

#include <stdint.h> // AWH
#ifdef CONFIG_TCG_TAINT
#include "taint_memory.h"

#endif /* CONFIG_TCG_TAINT*/
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


#define size_to_mask(size) ((1u << (size*8)) - 1u) //size<=4

#ifdef CONFIG_TCG_TAINT

static inline uint64_t taintcheck_register_check(int regid,int offset,int size,CPUState *env){
	int off = offset*8;
#if defined(TARGET_MIPS)
    return (size < 4) ? (env->active_tc.taint_gpr[regid]>>off)
        &size_to_mask(size):env->active_tc.taint_gpr[regid]>>off;
#else
    return (size < 4) ? (env->taint_regs[regid]>>off)&size_to_mask(size):
    		env->taint_regs[regid]>>off;
#endif /* CONFIG_MIPS */
}


int taintcheck_check_virtmem(gva_t vaddr, uint32_t size,uint8_t *taint);

int  taintcheck_taint_virtmem(gva_t vaddr, uint32_t size, uint8_t * taint);

void taintcheck_nic_writebuf(const uint32_t addr, const int size, const uint8_t * taint);

void taintcheck_nic_readbuf(const uint32_t addr, const int size, uint8_t *taint);

void taintcheck_nic_cleanbuf(const uint32_t addr, const int size);

void taintcheck_chk_hdout(const int size, const int64_t sect_num, const uint32_t offset, const void *s);

void taintcheck_chk_hdin(const int size, const int64_t sect_num, const uint32_t offset, const void *s);

void taintcheck_chk_hdwrite(const ram_addr_t paddr, const int size, const int64_t sect_num, const void *s);

void taintcheck_chk_hdread(const ram_addr_t paddr, const int size, const int64_t sect_num, const void *s);

int taintcheck_init(void);

void taintcheck_cleanup(void);

#endif /* CONFIG_TCG_TAINT */
#ifdef __cplusplus
}
#endif // __cplusplus

#endif //TAINTCHECK_OPT_H_INCLUDED
