/*
 * TraceConverterARM.cpp
 *
 *  Created on: Jul 8, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#include <sstream>
#include <iomanip>
#include <arpa/inet.h>
#include "TraceConverterARM.h"

using namespace std;

//static methods
int TraceConverterARM::convert(string& str, TRInstructionARM& insn, bool bIsThumb, bool bVerbose, bool bNeedHtonl)
{
  uint32_t data = insn.eh.header.insn;

  if (bNeedHtonl)
  {
    data = htonl(insn.eh.header.insn);
  }

  stringstream ss;

  disassemble_ARM(str, data, bIsThumb);

  //ss << "\ninsn: " << hex << setw(5) << setfill('0') << insn.eh.header.insn << ", eip: " << setw(8) << insn.eh.header.eip << endl;
  ss << hex << setw(8) << setfill ('0') << insn.eh.header.eip << ":\t" << str << "\t" << "RAW: " << data << endl;

  if (bVerbose)
  {
    for(size_t i = 0; (i < insn.eh.header.read_opers) && (i < TRINSTRUCTION_ARM_MAX_OPERANDS); i++)
    {
            ss << "Read Oper[" << i << "]. Type = " << insn.eh.read[i].type << ", Addr = " << hex << setw(8) << setfill('0') << insn.eh.read[i].addr << ", Val = " << insn.eh.read[i].val << endl;
    }

    for(size_t i = 0; (i < insn.eh.header.write_opers) && (i < TRINSTRUCTION_ARM_MAX_OPERANDS); i++)
    {
            ss << "Write Oper[" << i << "]. Type = " << insn.eh.write[i].type << ", Addr = " << hex << setw(8) << setfill('0') << insn.eh.write[i].addr << ", Val = " << insn.eh.write[i].val << endl;
    }
  }
  //str.append(ss.str());
  str = ss.str();

  return (0);
}

int TraceConverterARM::convert(TRInstructionARM& insn, const string& str)
{
  return (-1);
}

//member methods
int TraceConverterARM::convert(const TRInstructionARM& i)
{
  mInsn = i;
  //return (TraceConverterARM::convert(mStrInsn, mInsn));
}

int TraceConverterARM::convert(const string& str)
{
  mStrInsn = str;
  //return (TraceConverterARM::convert(mInsn, mStrInsn));
}

