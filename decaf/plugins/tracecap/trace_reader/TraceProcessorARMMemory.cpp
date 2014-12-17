/*
 * TraceProcessorARMMemory.cpp
 *
 *  Created on: Jul 11, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#include "TraceProcessorARMMemory.h"

#include <iostream>

int TraceProcessorARMMemory::processInstruction(const TRInstructionARM& insn)
{
  const EntryHeaderARM& eh = insn.eh;

  for (size_t t = 0; (t < eh.header.read_opers) && (t < TRINSTRUCTION_ARM_MAX_OPERANDS); t++)
  {
    if (eh.read[t].type == ARM_MEM_TYPE_MEM)
    {
      addItem(eh.read[t].addr, (uint8_t*)(&(eh.read[t].val)), 4);
    }
  }

  for (size_t t = 0; (t < eh.header.write_opers) && (t < TRINSTRUCTION_ARM_MAX_OPERANDS); t++)
  {
    if (eh.write[t].type == ARM_MEM_TYPE_MEM)
    {
      addItem(eh.write[t].addr, (uint8_t*)(&(eh.write[t].val)), 4);
    }
  }
  return (0);
}

TraceProcessorARMMemory::TraceProcessorARMMemory()
{
  // TODO Auto-generated constructor stub

}

TraceProcessorARMMemory::~TraceProcessorARMMemory()
{
  // TODO Auto-generated destructor stub
}
