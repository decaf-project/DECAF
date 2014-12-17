/*
 * TraceConverterARM.h
 *
 *  Created on: Jul 8, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#ifndef TRACECONVERTERARM_H_
#define TRACECONVERTERARM_H_

#include <string>
#include "TRInstructionARM.h"
#include "LibOpcodesWrapper_ARM.h"

class TraceConverterARM
{
public:
  TraceConverterARM() {}
  int convert(const TRInstructionARM& i);
  int convert(const std::string& str); //this is NOT a perfect conversion since insn->str uses the sorting algorithm, which is not reversible
  const std::string& getString() const { return (mStrInsn); }
  const TRInstructionARM& getInsn() const { return (mInsn); }
  std::string& getString() { return (mStrInsn); }
  TRInstructionARM& getInsn() { return (mInsn); }

  static int convert(std::string& str, TRInstructionARM& i, bool bIsThumb, bool bVerbose, bool bNeedHtonl = false);
  static int convert(TRInstructionARM& insn, const std::string& str);
private:

  std::string mStrInsn;
  TRInstructionARM mInsn;
};

#endif /* TRACECONVERTERARM_H_ */
