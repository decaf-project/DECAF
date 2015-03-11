/*
 * TraceConverterX86.h
 *
 *  Created on: Jun 9, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#ifndef TRACECONVERTERX86_H_
#define TRACECONVERTERX86_H_

#include <string>
#include <iostream>
#include <inttypes.h>
#include "TRInstructionX86.h"


#define LINE_SIZE 1024

/** strings for information output. */
const static char optype_map[7] = {'N', 'R', 'M', 'I', 'J', 'F', 'A'};
const static char access_map[8][4] = {{""}, {"RW"}, {"R"}, {"W"}, {"RCW"}, {"CW"}, {"CRW"}, {"CR"}};
const static char regname_map[40][4] = {
		{"es"}, {"cs"}, {"ss"}, {"ds"}, {"fs"}, {"gs"}, {""}, {""},
		{""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
        {"al"}, {"cl"}, {"dl"}, {"bl"}, {"ah"}, {"ch"}, {"dh"}, {"bh"},
        {"ax"}, {"cx"}, {"dx"}, {"bx"}, {"sp"}, {"bp"}, {"si"}, {"di"},
        {"eax"}, {"ecx"}, {"edx"}, {"ebx"}, {"esp"}, {"ebp"}, {"esi"}, {"edi"}};


class TraceConverterX86
{
public:
  TraceConverterX86() : bVerbose(true) {}
  int convert(const TRInstructionX86& i);
  int convert(const std::string& str); //this is NOT a perfect conversion since insn->str uses the sorting algorithm, which is not reversible
  int convert(const EntryHeader& eh);
  int convert(const EntryHeader* peh);
  const std::string& getString() { return (mStrInsn); }
  const TRInstructionX86& getInsn() { return (mInsn); }
  void setVerbose(bool b) { bVerbose = b; }

  static int convert(std::string& str, const TRInstructionX86& i, bool bVerbose = true);
  static int convert(TRInstructionX86& insn, const std::string& str);
  static void operandToString(std::string& str, const OperandVal& op, bool fullRegisterName = false);
private:
  //function that is needed for sorting the operands
  //friend bool operand_compare(const OperandVal& op1, const OperandVal& op2);

  static void printOperand(const OperandVal& op, std::ostream& outs);
  static void printInstruction(std::ostream&, const TRInstructionX86& insn, bool bOps = true, bool bVerbose = true);
  static uint32_t regStrToAddr(const std::string& str);

  bool bVerbose;
  std::string mStrInsn;
  TRInstructionX86 mInsn;
};

#endif /* TRACECONVERTERX86_H_ */
