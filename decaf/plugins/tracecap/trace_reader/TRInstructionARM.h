/**
 *  @Author Lok Yan
 */
#ifndef TRINSTRUCTION_ARM_H
#define TRINSTRUCTION_ARM_H

#include <inttypes.h>
#include "libdisarm_disarm.h"

enum arm_addr_type
{
  ARM_MEM_TYPE_REG = 0,
  ARM_MEM_TYPE_MEM = 1
};

#define TRINSTRUCTION_ARM_MAX_OPERANDS 40

struct ARMInsnHeader{
        uint32_t deadbeef;
        uint64_t entry_num;
        uint32_t eip;
        uint32_t insn;
        uint32_t read_opers;
        uint32_t write_opers;
};

struct ARMInsnOperand{
        arm_addr_type type;
        uint32_t addr;
        uint32_t val;
        uint32_t size;
};

struct EntryHeaderARM{
        ARMInsnHeader header;
        ARMInsnOperand read[TRINSTRUCTION_ARM_MAX_OPERANDS];
        ARMInsnOperand write[TRINSTRUCTION_ARM_MAX_OPERANDS];
};

struct TRInstructionARM
{

  EntryHeaderARM eh;

  struct
  {
    da_instr_t instr;
    da_instr_args_t args;
  } insn;
};

#endif//TRINSTRUCTION_ARM_H
