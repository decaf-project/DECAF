/**
 *  @Author Lok Yan
 */
#include "TraceProcessorX86CPUState.h"

//#include <iostream>

using namespace std;

int TraceProcessorX86CPUState::processInstruction(const TRInstructionX86& insn)
{
  int writeOp = -1;
  uint32_t newData = 0;

  for (int i = 0; (insn.eh.operand[i].type != TNone) && (i < MAX_NUM_OPERANDS); i++)
  {
    if (insn.eh.operand[i].type == TRegister)
    {
      writeRegister(insn.eh.operand[i].addr, insn.eh.operand[i].value, insn.eh.operand[i].length);
      if ( (insn.eh.operand[i].access == XED_OPERAND_ACTION_W) )
      {
        writeOp = i;
      }
    }

    if (insn.eh.operand[i].access == XED_OPERAND_ACTION_R)
    {
      newData = insn.eh.operand[i].value;
    }
  }
  if (writeOp >= 0)
  {
    if (writeRegister(insn.eh.operand[writeOp].addr, newData, insn.eh.operand[writeOp].length))
    {
      return (-1);
    }
  }
  //cout << hex << cpu.gen.eax <<  endl;
  return (0);
}

int TraceProcessorX86CPUState::processInstruction(const string& str)
{
  int ret = tc.convert(str);
  if (ret)
  {
    return (ret);
  }

  return (processInstruction(tc.getInsn()));
}

int TraceProcessorX86CPUState::writeRegister(uint32_t addr, uint32_t value, size_t size)
{
  // general purpose registers
  if ( (addr >= al_reg) && (addr <= bl_reg) && (size == 1) )
  {
    //8 bit registers
    cpu.gen.arr[addr - al_reg] &= 0xFFFFFF00;
    cpu.gen.arr[addr - al_reg] |= (0xFF & value);
    return (0);
  }
  if ( (addr >= ah_reg) && (addr <= bh_reg) && (size == 1) )
  {
    //8 bit registers
    cpu.gen.arr[addr - ah_reg] &= 0xFFFF00FF;
    cpu.gen.arr[addr - ah_reg] |= ((0xFF & value) << 8);
    return (0);
  }
  if ( (addr >= ax_reg) && (addr <= bx_reg) && (size == 2) )
  {
    //16 bit registers
    cpu.gen.arr[addr - ax_reg] &= 0xFFFF0000;
    cpu.gen.arr[addr - ax_reg] |= (0xFFFF & value);
    return (0);
  }
  if ( (addr >= eax_reg) && (addr <= ebx_reg) && (size == 4) )
  {
    //32 bit registers
    cpu.gen.arr[addr - eax_reg] = value;
    return (0);
  }

  //stack registers
  if ( (addr >= sp_reg) && (addr <= di_reg) && (size == 2) )
  {
    cpu.stk.arr[addr - sp_reg] &= 0xFFFF0000;
    cpu.stk.arr[addr - sp_reg] |= (0xFFFF & value);
    return (0);
  }
  if ( (addr >= esp_reg) && (addr <= edi_reg) && (size == 4) )
  {
    cpu.stk.arr[addr - esp_reg] = value;
    return (0);
  }

  //segment registers
  if ( (addr >= es_reg) && (addr <= gs_reg) && (size == 4) )
  {
    cpu.segregs.arr[addr - es_reg] = value;
    return (0);
  }

  return (-1);
}
