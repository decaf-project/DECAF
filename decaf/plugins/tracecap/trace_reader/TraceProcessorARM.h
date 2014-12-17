/*
 * TraceProcessorARM.h
 *
 *  Created on: Jul 11, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORARM_H_
#define TRACEPROCESSORARM_H_

#include <string>
#include "TRInstructionARM.h"
#include "History.h"
#include "Common.h"

class TraceProcessorARM
{
public:
  virtual int processInstruction(History<TRInstructionARM>& insns) = 0;
  virtual int processInstruction(History<std::string>& strInsns) = 0;
  virtual bool isInterested(const std::string& s) = 0;
  virtual bool isInterested(const TRInstructionARM& insn) = 0;
  const std::string& getResult() { return (strEmpty); }
};

#endif /* TRACEPROCESSORARM_H_ */
