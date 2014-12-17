/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORX86INTERRUPTLINUX_H
#define TRACEPROCESSORX86INTERRUPTLINUX_H

#include "TraceProcessorX86.h"
#include "X86CPUState.h"

//As of now the interrupts are based on "mov XXXX, %eax; int 0x80"
// which might not cover everything, it is probably better and more correct
// to rebuild the register states and get the interrupt values from there
class TraceProcessorX86InterruptLinux : public TraceProcessorX86
{
public:
  int processInstruction(History<TRInstructionX86>& insns);
  int processInstruction(History<std::string>& strs);
  int processInstruction(const TRInstructionX86& insn, const X86CPUState& cpu);
  int processInstruction(const std::string& str, const X86CPUState& cpu);
  int processInstruction(History<TRInstructionX86>& insns, const X86CPUState& cpu) { return (processInstruction(insns.at(0), cpu)); }
  int processInstruction(History<std::string>& strs, const X86CPUState& cpu) { return (processInstruction(strs.at(0), cpu)); }
  bool isInterested(const std::string& s);
  bool isInterested(const TRInstructionX86& insn);
  const std::string& getResult() { return (curSyscallName); }
private:
  std::string curSyscallName;
};

#endif//TRACEPROCESSORX86INTERRUPTLINUX_H
