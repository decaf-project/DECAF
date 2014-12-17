/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
           Zhenkai Liang <liangzk@comp.nus.edu.sg>
*/
#include <sys/time.h>
#include <assert.h>
#include "operandinfo.h"
#include "config.h"
#include "tracecap.h"
#include "DECAF_main.h"
#include "shared/tainting/taintcheck_opt.h"
/* Flag that states if tainted data has already been seen during trace */

int received_tainted_data = 0;
#ifdef CONFIG_TCG_TAINT
/* Copy the given taint record into the given operand
     and check whether this is the first tainted operand seen
*/
inline void record_taint_value(OperandVal * op) {
	struct timeval ftime;

	if (0 == received_tainted_data) {
		received_tainted_data = 1;
		if (gettimeofday(&ftime, 0) == 0) {
			monitor_printf(default_mon, "Time of first tainted data: %ld.%ld\n",
					ftime.tv_sec, ftime.tv_usec);
		}

		}

}

//Hacker way to store bitwise tainting info using opval.records[0].taintBytes[0].source = BIT_TAINT

void store_bitwise_taint(OperandVal *op, uint64_t taint)
{
	taint_record_t trt;
	TaintByteRecord tbr;
	memset(&trt, 0, sizeof(taint_record_t));
	memset(&tbr, 0, sizeof(TaintByteRecord));
	tbr.source = (uint32_t) (taint>>32 & 0xFFFFFFFF);
	tbr.origin = (uint32_t) (taint & 0xFFFFFFFF);
    trt.numRecords = 1;
    trt.taintBytes[0] = tbr;
    op->records[0] = trt;


}
uint16_t taintcheck_check_virtmem_compress(OperandVal *op, uint32_t addr, uint32_t size)
{
	uint8_t taint[8] = { 0 };
	uint16_t ret = 0;
	int i;
	taintcheck_check_virtmem(addr, size, taint);
	for (i = 0; i < size; i++) {
		if (taint[i])
			ret |= 1 << i;
	}
	if(ret)
		store_bitwise_taint(op,(uint64_t )*taint);
	return ret;
}

uint16_t taintcheck_check_reg_compress(OperandVal *op, uint32_t regnum,uint32_t offset,uint32_t size){
	uint64_t taint = 0;
	int i;
	uint16_t ret = 0;
	taint = taintcheck_register_check(regnum, offset, size, cpu_single_env);
	for (i = 0; i < size; i++) {
		if (taint & (0xFF << 8 * i))
			ret |= 1 << i;
	}
	if(ret)
		store_bitwise_taint(op,taint);
	return ret;

}

int get_rand()
{  
   srand(time(NULL));
   int x = rand() & 0xffff;
   x |= (rand() & 0xffff) << 16;
   return x;
}
#define size_to_mask(size) ((1u << (size*8)) - 1u)
void taint_reg(OperandVal *op, uint32_t regnum,uint32_t offset,uint32_t size)
{
	    int taint1 =get_rand();
	    int off = offset*8;
	    if(size < 4)
	    	cpu_single_env->taint_regs[regnum] = (taint1>>off)&size_to_mask(size);
	    else
	    	cpu_single_env->taint_regs[regnum] =taint1;



}
void taint_virt_mem(OperandVal *op, uint32_t addr, uint32_t size)
{
	uint32_t i,phy_addr;
	uint8_t taint;
	for (i = 0;i<size;i++){
		phy_addr =  DECAF_get_phys_addr(cpu_single_env, addr+i);
		taint = get_rand();
		taint_mem(phy_addr,1,&taint);
	}

}
#endif
void set_operand_data_begin(OperandVal *op)
{
#if CONFIG_TCG_TAINT
	switch (op->type) {
		case TRegister: {
			int regnum = REGNUM(op->addr);
			int offset = getOperandOffset(op);
			if(regnum!=-1)
				op->tainted_begin = taintcheck_register_check(regnum, offset, op->length ,cpu_single_env);
			break;
		}
		case TMemLoc: {
				taintcheck_check_virtmem(op->addr,op->length, (uint8_t *)&(op->tainted_begin));
			break;
		}
		default:
		op->tainted_begin = 0;
		break;
	}

	if (op->tainted_begin!=0) {
		insn_tainted=1;
		record_taint_value(op);
	}
#else
	op->tainted_begin = 0;
#endif
}


void set_operand_data_end(OperandVal *op)
{
#if CONFIG_TCG_TAINT
	switch (op->type) {
		case TRegister: {
			int regnum = REGNUM(op->addr);
			int offset = getOperandOffset(op);
			if(regnum!=-1)

			op->tainted_end = taintcheck_register_check(regnum, offset, op->length ,cpu_single_env);  //bit wise tainting
			break;
		}
		case TMemLoc: {

			taintcheck_check_virtmem(op->addr,op->length, (uint8_t *)&(op->tainted_end));//bit wise tainting
			break;
		}
		default:
		op->tainted_end = 0;
		break;
	}
	if (op->tainted_end) {

		insn_tainted=1;
		record_taint_value(op);
	}
#else
	op->tainted_end = 0;
#endif
}


/* Given an operand, check taint information and store it */
void set_operand_data(OperandVal *op,int is_begin) {
	if(is_begin)
		set_operand_data_begin(op);
	else
		set_operand_data_end(op);

}
