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
 * @date 12 OCT 2012
 */

#ifndef DECAF_CALLBACK_COMMON_H
#define DECAF_CALLBACK_COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "cpu.h"
#include "shared/DECAF_types.h"

/*
 * Condition flags for range callbacks. Transitions are only valid for instructions that branch.
 * For all other instructions only 0x00, 0x01, 0x08 are valid.
 */
typedef enum {
	DECAF_ALL					= 0x00,
	DECAF_USER_TO_USER_ONLY     = 0x01,
	DECAF_USER_TO_KERNEL_ONLY   = 0x02,
	DECAF_KERNEL_TO_USER_ONLY   = 0x04,
	DECAF_KERNEL_TO_KERNEL_ONLY = 0x08,
} OpcodeRangeCallbackConditions;

typedef enum {
        DECAF_BLOCK_BEGIN_CB = 0,
        DECAF_BLOCK_END_CB,
        DECAF_INSN_BEGIN_CB,
        DECAF_INSN_END_CB,
        DECAF_MEM_READ_CB,
        DECAF_MEM_WRITE_CB,
        DECAF_EIP_CHECK_CB,
        DECAF_KEYSTROKE_CB,//keystroke event
        DECAF_NIC_REC_CB,
        DECAF_NIC_SEND_CB,
        DECAF_OPCODE_RANGE_CB,
        DECAF_TLB_EXEC_CB,
        DECAF_READ_TAINTMEM_CB,
        DECAF_WRITE_TAINTMEM_CB,
#ifdef CONFIG_TCG_LLVM
	DECAF_BLOCK_TRANS_CB,
#endif /* CONFIG_TCG_LLVM */
        DECAF_LAST_CB, //place holder for the last position, no other uses.
} DECAF_callback_type_t;


//Optimized Callback type
typedef enum _OCB_t {
  /**
   * Optimized Callback Condition - Const - The value associated with this flag needs an exact match
   */
  OCB_CONST = 2,
  /**
   * Optimized callback Condition - Page - A match is found as long as the page numbers match
   */
  OCB_PAGE = 4,
  /**
   * Not used yet
   */
  OCB_CONST_NOT = 3,
  /**
   * Not used yet
   */
  OCB_PAGE_NOT = 5,
  /**
   * Optimized Callback Condition - Everything!
   */
  OCB_ALL = -1
} OCB_t;
// HU- for memory read/write callback.Memory be read/written at different grains
//(byte,word,long,quad)
typedef enum{
	DECAF_BYTE=1,
	DECAF_WORD=2,
	DECAF_LONG=4,
	DECAF_QUAD=8,
} DATA_TYPE;

typedef struct _DECAF_Block_Begin_Params
{
  CPUState* env;
  TranslationBlock* tb;
}DECAF_Block_Begin_Params;

typedef struct _DECAF_Tlb_Exec_Params
{
	CPUState *env;
	gva_t vaddr;  //Address loaded to tlb exec cache
} DECAF_Tlb_Exec_Params;

typedef struct _DECAF_Opcode_Range_Params
{
	CPUState *env;
	gva_t eip;
	gva_t next_eip;
	uint16_t op;
} DECAF_Opcode_Range_Params;

typedef struct _DECAF_Block_End_Params
{
  CPUState* env;
  TranslationBlock* tb;
  //THIS IS A PC value - NOT EIP!!!!
  gva_t cur_pc;
  gva_t next_pc;
} DECAF_Block_End_Params;

typedef struct _DECAF_Insn_Begin_Params
{
  CPUState* env;
} DECAF_Insn_Begin_Params;

typedef struct _DECAF_Insn_End_Params
{
  CPUState* env;
} DECAF_Insn_End_Params;
typedef struct _DECAF_Mem_Read_Params
{
	gva_t vaddr;
	gpa_t paddr;
	DATA_TYPE dt;

}DECAF_Mem_Read_Params;
typedef struct _DECAF_Mem_Write_Params
{
	gva_t vaddr;
	gpa_t paddr;
	DATA_TYPE dt;
}DECAF_Mem_Write_Params;
typedef struct _DECAF_EIP_Check_Params
{
	gva_t source_eip;
	gva_t target_eip;
    gva_t target_eip_taint;
}DECAF_EIP_Check_Params;
typedef struct _DECAF_Keystroke_Params
{
	int32_t keycode;
	uint32_t *taint_mark;//mark if this keystroke should be monitored

}DECAF_Keystroke_Params;

typedef struct _DECAF_Nic_Rec_Params
{
	uint8_t *buf;
	int32_t size;
	int32_t cur_pos;
	int32_t start;
	int32_t stop;
}DECAF_Nic_Rec_Params;

typedef struct _DECAF_Nic_Send_Params
{
	uint32_t addr;
	int size;
	uint8_t *buf;
}DECAF_Nic_Send_Params;

typedef struct _DECAF_Read_Taint_Mem
{
	gva_t vaddr;
	gpa_t paddr;
	uint32_t size;
	uint8_t *taint_info;

}DECAF_Read_Taint_Mem;

typedef struct _DECAF_Read_Write_Mem
{
	gva_t vaddr;
	gpa_t paddr;
	uint32_t size;
	uint8_t *taint_info;
}DECAF_Write_Taint_Mem;

#ifdef CONFIG_TCG_LLVM
typedef struct _DECAF_Block_Trans_Params
{
	struct TranslationBlock *tb;
	struct TCGContext *tcg_ctx;
}DECAF_Block_Trans_Params;
#endif /* CONFIG_TCG_LLVM */
//LOK: A dummy type
typedef struct _DECAF_Callback_Params
{
	DECAF_Handle cbhandle;
	union{
		DECAF_Block_Begin_Params bb;
		DECAF_Block_End_Params be;
		DECAF_Insn_Begin_Params ib;
		DECAF_Insn_End_Params ie;
		DECAF_Mem_Read_Params mr;
		DECAF_Mem_Write_Params mw;
		DECAF_EIP_Check_Params ec;
		DECAF_Keystroke_Params ks;
		DECAF_Nic_Rec_Params nr;
		DECAF_Nic_Send_Params ns;
		DECAF_Opcode_Range_Params op;
		DECAF_Tlb_Exec_Params tx;
		DECAF_Read_Taint_Mem rt;
		DECAF_Write_Taint_Mem wt;
#ifdef CONFIG_TCG_LLVM
		DECAF_Block_Trans_Params bt;
#endif /* CONFIG_TCG_LLVM */
	};
} DECAF_Callback_Params;

typedef void (*DECAF_callback_func_t)(DECAF_Callback_Params*);


#ifdef __cplusplus
}
#endif

#endif//DECAF_CALLBACK_COMMON_H
