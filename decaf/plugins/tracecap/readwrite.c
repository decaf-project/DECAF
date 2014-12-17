/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
*/
#include "readwrite.h"
#include "config.h"
#include "tracecap.h"
#include "shared/DECAF_main.h"
#include "DECAF_target.h"
#include "assert.h"
#include "xed-interface.h"

void get_new_access (
  xed_operand_action_enum_t curr_access,
  xed_operand_action_enum_t *old_access, 
  xed_operand_action_enum_t *new_access) 
{
  switch(curr_access) {
    case XED_OPERAND_ACTION_RW: {
      *old_access = XED_OPERAND_ACTION_R;
      *new_access = XED_OPERAND_ACTION_W;
      break;
    }
    case XED_OPERAND_ACTION_RCW: {
      *old_access = XED_OPERAND_ACTION_R;
      *new_access = XED_OPERAND_ACTION_CW;
      break;
    }
    case XED_OPERAND_ACTION_CRW: {
      *old_access = XED_OPERAND_ACTION_CR;
      *new_access = XED_OPERAND_ACTION_W;
      break;
    }
    default: {
      *old_access = curr_access;
      *new_access = curr_access;
      break;
    }
  }
 
}

static void update_operand_contents(OperandVal *opnd) {
  if (opnd->type == TRegister) {
    int regnum = get_regnum(*opnd);
    switch (opnd->addr) {
    case ax_reg: case bx_reg: case cx_reg: case dx_reg:
    case bp_reg: case sp_reg: case si_reg: case di_reg:
      opnd->value = cpu_single_env->regs[regnum] & 0xFFFF;
      break;
    case al_reg: case bl_reg: case cl_reg: case dl_reg:
      opnd->value = cpu_single_env->regs[regnum] & 0xFF;
      break;
    case ah_reg: case bh_reg: case ch_reg: case dh_reg:
      opnd->value = (cpu_single_env->regs[regnum] & 0xFF00) >> 8;
      break;
    default:
      opnd->value = cpu_single_env->regs[regnum];
      break;
    }
  }
  else if (opnd->type == TMemLoc) {
    DECAF_read_mem(cpu_single_env,opnd->addr, (int)(opnd->length),
		  (uint8_t *)&(opnd->value));
  }
}

// Update all written operands with current value
void update_written_operands (EntryHeader *eh) {
  int i = 0, first_empty_idx = 0;
  xed_operand_action_enum_t old_access, new_access;
  

    // Find number of operands
    while ((eh->operand[i].type != TNone) && (i < MAX_NUM_OPERANDS)) {
      i++;
    }
    first_empty_idx = i;

    // Modify operands
    i = 0;
    while ((eh->operand[i].type != TNone) && (i < MAX_NUM_OPERANDS)) {
      switch(eh->operand[i].access) {
        case XED_OPERAND_ACTION_W:
        case XED_OPERAND_ACTION_CW: {
          // Just update the operand value
	  update_operand_contents(&(eh->operand[i]));
          break;
	}
        case XED_OPERAND_ACTION_RW:
        case XED_OPERAND_ACTION_RCW:
        case XED_OPERAND_ACTION_CRW: {
          // Copy operand to empty slot
          assert(first_empty_idx < MAX_NUM_OPERANDS);
          assert(first_empty_idx != i);
          memcpy((void *)&(eh->operand[first_empty_idx]),
	    (void *)&(eh->operand[i]),sizeof(OperandVal)); 

          // Update the number of operands
          eh->num_operands++;

          // Update operand access for both operands
          get_new_access(eh->operand[i].access,&old_access,&new_access);
	  eh->operand[i].access = old_access;
	  eh->operand[first_empty_idx].access = new_access;

          // Update value for new operand
	  update_operand_contents(&(eh->operand[first_empty_idx]));

          first_empty_idx++;
	  break;
	}
        default: {
          break;
	}
      }

      i++;
    }

}

