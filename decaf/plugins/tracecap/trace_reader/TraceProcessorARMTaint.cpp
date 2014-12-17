/*
 * TraceProcessorARMTaint.cpp
 *
 *  Created on: Jul 12, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#include <iostream>
#include <iomanip>
#include "TraceProcessorARMTaint.h"

void TraceProcessorARMTaint::printTaints(std::ostream& os)
{
  taint_container_t::const_iterator it;
  for (it = mMemTaints.begin(); it != mMemTaints.end(); it++)
  {
    os << std::hex << std::setw(8) << std::setfill('0') << std::showbase << (it->first) << " :  " << std::dec;
    printTaints(os, it->second);
    os << std::endl;
  }

  for (it = mRegTaints.begin(); it != mRegTaints.end(); it++)
  {
    os << std::hex << std::setw(8) << std::setfill('0') << std::showbase << (it->first) << " :  " << std::dec;
    printTaints(os, it->second);
    os << std::endl;
  }
}

void TraceProcessorARMTaint::printTaints(std::ostream& os, const taint_container_t::taints_set_t& taintSet)
{
  taint_container_t::taints_set_t::const_iterator it;
  for (it = taintSet.begin(); it != taintSet.end(); it++)
  {
    os << *it << ", ";
  }
}

int TraceProcessorARMTaint::processInstruction(const TRInstructionARM& insn)
{
  const EntryHeaderARM& eh = insn.eh;

  taint_container_t::taints_set_t memtaints;
  taint_container_t::taints_set_t regtaints;

  for (size_t t = 0; (t < eh.header.read_opers) && (t < TRINSTRUCTION_ARM_MAX_OPERANDS); t++)
  {
    if (eh.read[t].type == ARM_MEM_TYPE_MEM)
    {
      if (mMemTaints.find(eh.read[t].addr) != mMemTaints.end())
      {
        taint_container_t::orTaints(memtaints, (mMemTaints.find(eh.read[t].addr))->second);
      }
    }
    else if (eh.read[t].type == ARM_MEM_TYPE_REG)
    {
      if (mRegTaints.find(eh.read[t].addr) != mRegTaints.end())
      {
        taint_container_t::orTaints(regtaints, (mRegTaints.find(eh.read[t].addr))->second);
      }
    }
  }

  for (size_t t = 0; (t < eh.header.write_opers) && (t < TRINSTRUCTION_ARM_MAX_OPERANDS); t++)
  {
    if ( (eh.read[t].type == ARM_MEM_TYPE_MEM) && (!memtaints.empty()) )
    {
      taint_container_t::orTaints(mMemTaints[eh.write[t].addr], memtaints);
    }
    else if ( (eh.read[t].type == ARM_MEM_TYPE_REG) && (!regtaints.empty()) )
    {
      taint_container_t::orTaints(mRegTaints[eh.write[t].addr], regtaints);
    }
  }

  return (0);
}
