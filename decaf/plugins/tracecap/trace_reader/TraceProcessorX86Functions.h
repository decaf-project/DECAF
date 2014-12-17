/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORX86FUNCTIONS_H
#define TRACEPROCESSORX86FUNCTIONS_H

#include "TraceProcessorX86.h"
#include "SymbolMap.h"

class TraceProcessorX86Functions : public TraceProcessorX86
{
public:
  TraceProcessorX86Functions(SymbolMap& newSM) : sm(newSM) {}
  int processInstruction(const TRInstructionX86& insn);
  int processInstruction(History<TRInstructionX86>& insns) { return (processInstruction(insns.at(0))); }
  int processInstruction(History<std::string>& strs) { return (processInstruction(strs.at(0))); }
  int processInstruction(const std::string& s);
  const std::string& getResult() { return (curFunctionName); }
  bool isInterested(const std::string& str);
  bool isInterested(const TRInstructionX86& insn);
  size_t getJmpPositionInLine(const std::string& s);
  size_t getCallPositionInLine(const std::string& s);
protected:
  SymbolMap& sm;
  std::string curFunctionName;
};

#endif//TRACEPROCESSORX86FUNCTIONS_H
