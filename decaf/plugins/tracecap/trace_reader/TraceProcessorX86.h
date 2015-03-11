/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORX86_H
#define TRACEPROCESSORX86_H

#include <string>
#include "TRInstructionX86.h"
#include "History.h"
#include "Common.h"

class TraceProcessorX86
{
public:
//  virtual int processInstruction(History<TRInstructionX86>& insns) = 0;
//  virtual int processInstruction(History<std::string>& strInsns) = 0;
//  virtual bool isInterested(const std::string& s) = 0;
  virtual bool isInterested(const TRInstructionX86& insn) = 0;
  const std::string& getResult() { return (strEmpty); }
};

#endif//TRACEPROCESSORX86_H
