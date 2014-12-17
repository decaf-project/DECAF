/**
 *  @Author Lok Yan
 * @date 24 APR 2013
 */
#ifndef TRACEPROCESSORX86TAINTSUMMARY_H
#define TRACEPROCESSORX86TAINTSUMMARY_H

#include <list>
#include "TraceProcessorX86.h"
#include "trace_x86.h"

struct TaintEntry
{
  uint32_t insn_addr;
  uint64_t inputTaint;
  uint64_t outputTaint;
  uint32_t insnNum;
  OperandVal op;
  bool bReady; //true if this taintentry is complete (that is the outputTaint is known)
};

class TraceProcessorX86TaintSummary : public TraceProcessorX86
{
public:
  TraceProcessorX86TaintSummary(bool bitTaint = false) : prNumInsns(0), prbBitTaint(bitTaint) {}
  int processInstruction(const TRInstructionX86& insns);
  int processInstruction(const std::string& strInsns) { return (-1); }
  int processInstruction(History<TRInstructionX86>& insns) { return processInstruction(insns.at(0)); }
  int processInstruction(History<std::string>& strs) { return (processInstruction(strs.at(0))); }
  bool isInterested(const std::string& s) { return (false); }
  bool isInterested(const TRInstructionX86& insn) { return (true); }
  const std::string& getResult() { return (prResult); }

private:
  bool isSameRegister(uint32_t regnum1, uint32_t regnum2);
  int updateQueue(uint32_t addr, const OperandVal& op, bool bShouldAdd);

  bool prbBitTaint;
  std::list<TaintEntry*> prQ; //we use a list instead of a deque since we 
  // expect to remove items at any location
  std::string prResult;
  uint32_t prNumInsns;
  //ChildPiper prCP; //NOT USED UNTIL WE FIX THE BAP SPAWNING ERROR
};

#endif//TRACEPROCESSORX86VERIFY_H
