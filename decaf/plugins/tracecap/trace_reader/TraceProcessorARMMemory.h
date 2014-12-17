/*
 * TraceProcessorARMMemory.h
 *
 *  Created on: Jul 11, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORARMMEMORY_H_
#define TRACEPROCESSORARMMEMORY_H_

#include "TRInstructionARM.h"
#include "TraceProcessorARM.h"
#include "MemoryBuilder.h"

class TraceProcessorARMMemory : public TraceProcessorARM, public MemoryBuilder
{
public:
  TraceProcessorARMMemory();
  int processInstruction(const TRInstructionARM& insn);
  int processInstruction(const std::string& str) { return (-1); }
  int processInstruction(History<TRInstructionARM>& insns) { return processInstruction(insns.at(0)); }
  int processInstruction(History<std::string>& strs) { return (processInstruction(strs.at(0))); }
  bool isInterested(const std::string& s) { return (false); }
  bool isInterested(const TRInstructionARM& insn) { return (true); }
  virtual
  ~TraceProcessorARMMemory();
};

#endif /* TRACEPROCESSORARMMEMORY_H_ */
