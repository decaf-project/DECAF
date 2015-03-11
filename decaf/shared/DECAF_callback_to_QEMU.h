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
 * @date 9 Oct 2012
 * Explicit declaration of prototypes between DECAF callbacks and QEMU. This includes all of the
 *   helper functions
 */

#ifndef DECAF_CALLBACK_TO_QEMU_H
#define DECAF_CALLBACK_TO_QEMU_H

#ifdef __cplusplus
extern "C"
{
#endif

// #include "cpu.h" //Not needed - included in DECAF_callback_common.h
// #include "shared/DECAF_types.h" // not needed either
#include "shared/DECAF_callback_common.h"

int DECAF_is_callback_needed(DECAF_callback_type_t cb_type);
int DECAF_is_callback_needed_for_opcode(int op);
int DECAF_is_BlockBeginCallback_needed(gva_t pc);
int DECAF_is_BlockEndCallback_needed(gva_t from, gva_t to);

//This is needed since tlb_exec_cb doesn't go into tb and therefore not in helper.h
#ifdef CONFIG_VMI_ENABLE
void DECAF_invoke_tlb_exec_callback(CPUState *env, gva_t vaddr);
#endif

void helper_DECAF_invoke_nic_rec_callback(const uint8_t * buf, int size, int cur_pos, int start, int stop);
void helper_DECAF_invoke_nic_send_callback(uint32_t addr, int size, const uint8_t *buf);
void helper_DECAF_invoke_mem_read_callback(gva_t virt_addr,gpa_t phy_addr, unsigned long value, DATA_TYPE data_type);
void helper_DECAF_invoke_mem_write_callback(gva_t virt_addr,gpa_t phy_addr,unsigned long value, DATA_TYPE data_type);
void helper_DECAF_invoke_keystroke_callback(int keycode,uint32_t *taint_mark);
void helper_DECAF_invoke_read_taint_mem(gva_t vaddr,gpa_t paddr,uint32_t size,uint8_t *taint_info);
void helper_DECAF_invoke_write_taint_mem(gva_t vaddr,gpa_t paddr,uint32_t size,uint8_t *taint_info);
void helper_DECAF_invoke_log_pointer_read(gva_t virt_addr,gva_t taint_info);
void helper_DECAF_invoke_log_pointer_write(gva_t virt_addr, gva_t taint_info);
#ifdef CONFIG_TCG_LLVM
void helper_DECAF_invoke_block_trans_callback(const struct TranslationBlock *tb, const struct TCGContext *tcg_ctx);
#endif /* CONFIG_TCG_LLVM */
//The following prototypes are not needed since they are defined in
// helper.h
//void helper_DECAF_invoke_block_begin_callback(CPUState* env, TranslationBlock* tb);
//void helper_DECAF_invoke_block_end_callback(CPUState* env, TranslationBlock* tb, gva_t from);
//void helper_DECAF_invoke_insn_begin_callback(CPUState* env);
//void helper_DECAF_invoke_insn_begin_callback(CPUState* env);

#ifdef __cplusplus
}
#endif

#endif//DECAF_CALLBACK_TO_QEMU_H
