/*
 * TraceProcessorARMTaint.h
 *
 *  Created on: Jul 12, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#ifndef TRACEPROCESSORARMTAINT_H_
#define TRACEPROCESSORARMTAINT_H_

#include <iostream>
#include "TraceProcessorARM.h"
#include "Taint.h"

class TraceProcessorARMTaint : public TraceProcessorARM
{
public:
  typedef Taint<uint32_t> taint_container_t;

  TraceProcessorARMTaint() {mRegTaints.addTaint(0, 123);}
  int processInstruction(const TRInstructionARM& insn);
  int processInstruction(const std::string& str) { return (-1); }
  int processInstruction(History<TRInstructionARM>& insns) { return (processInstruction(insns.at(0))); }
  int processInstruction(History<std::string>& strInsns) { return (processInstruction(strInsns.at(0))); }
  bool isInterested(const std::string& s) { return (false); }
  bool isInterested(const TRInstructionARM& insn) { return (true); }

  void printTaints(std::ostream& os);
  void printTaints(std::ostream& os, const taint_container_t::taints_set_t& taintSet);
private:
  taint_container_t mMemTaints;
  taint_container_t mRegTaints;
};

#endif /* TRACEPROCESSORARMTAINT_H_ */
