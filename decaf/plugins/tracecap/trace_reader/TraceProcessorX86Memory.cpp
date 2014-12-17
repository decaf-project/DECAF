/**
 *  @Author Lok Yan
 */
#include "TraceProcessorX86Memory.h"
#include "HelperFunctions.h"

using namespace std;

int TraceProcessorX86Memory::processInstruction(const TRInstructionX86& insn)
{
  addItem(insn.eh.address, (uint8_t*)insn.eh.rawbytes, insn.eh.inst_size);
  uint32_t newData = 0;
  int writeOp = -1;
  for (int i = 0; (insn.eh.operand[i].type != TNone) && (i < MAX_NUM_OPERANDS); i++)
  {
    /*
    if (insn.eh.operand[i].addr == 0x00142154)
    {
      cout << "AKSJDHFLKSJDHFLKSDJF" << endl;
    }
*/
    if (insn.eh.operand[i].type == TMemLoc)
    {
      //now that we know its a memory location lets get the operand and add the item
      addItem(insn.eh.operand[i].addr, (uint8_t*)(&(insn.eh.operand[i].value)),insn.eh.operand[i].length);
      //this works for now, but we will definitely need to figure out what to do with the conditional instructions

      if ( (insn.eh.operand[i].access == XED_OPERAND_ACTION_W) || (insn.eh.operand[i].access == XED_OPERAND_ACTION_CW) )
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
    //should this be newdata.length or as it is? shouldn't they be the same though?
    addItem(insn.eh.operand[writeOp].addr, (uint8_t*)(&(newData)),insn.eh.operand[writeOp].length);
  }
  return (0);
}

int TraceProcessorX86Memory::processInstruction(const string& str)
{
  int ret = tc.convert(str);

  if (ret)
  {
    return (ret);
  }

  return (processInstruction(tc.getInsn()));
}
