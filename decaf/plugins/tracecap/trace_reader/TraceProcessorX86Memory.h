/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORMEMORY_H
#define TRACEPROCESSORMEMORY_H

#include <string>
#include "MemoryBuilder.h"
#include "TraceProcessorX86.h"
#include "TraceConverterX86.h"

class TraceProcessorX86Memory : public TraceProcessorX86, public MemoryBuilder
{
public:
  TraceProcessorX86Memory() : MemoryBuilder() {} //empty constructor

  int processInstruction(const TRInstructionX86& insn);
  int processInstruction(const std::string& str);
  int processInstruction(History<TRInstructionX86>& insns) { return processInstruction(insns.at(0)); }
  int processInstruction(History<std::string>& strs) { return (processInstruction(strs.at(0))); }
  bool isInterested(const std::string& s) { return (true); }
  bool isInterested(const TRInstructionX86& insn) { return (true); }
private:
  TraceConverterX86 tc;
};

#endif//TRACEPROCESSORMEMORY_H
