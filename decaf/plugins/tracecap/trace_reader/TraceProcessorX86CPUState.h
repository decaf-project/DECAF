/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORX86CPUSTATE_H
#define TRACEPROCESSORX86CPUSTATE_H

#include "TraceProcessorX86.h"
#include "TraceConverterX86.h"
#include "X86CPUState.h"

class TraceProcessorX86CPUState : public TraceProcessorX86
{
public:
  int processInstruction(const TRInstructionX86& insns);
  int processInstruction(const std::string& strInsns);
  int processInstruction(History<TRInstructionX86>& insns) { return processInstruction(insns.at(0)); }
  int processInstruction(History<std::string>& strs) { return (processInstruction(strs.at(0))); }
  bool isInterested(const std::string& s) { return (true); }
  bool isInterested(const TRInstructionX86& insn) { return (true); }
  int writeRegister(uint32_t addr, uint32_t value, size_t size);

  const X86CPUState& getCPUState() { return (cpu); }

private:
  TraceConverterX86 tc;
  X86CPUState cpu;
};

#endif//TRACEPROCESSORX86CPUSTATE_H
