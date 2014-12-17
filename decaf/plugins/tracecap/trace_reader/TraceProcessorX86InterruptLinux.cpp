/**
 *  @Author Lok Yan
 */
#include "TraceProcessorX86InterruptLinux.h"
#include "HelperFunctions.h"
#include "syscalltable_linux_2_6.h"

#include "libdasm_part2.h"

//#include <iostream>

using namespace std;

int TraceProcessorX86InterruptLinux::processInstruction(History<TRInstructionX86>& insns)
{
  if (insns.size() < 2)
  {
    return (-1);
  }

  if (!isInterested(insns.at(0)))
  {
    return (-1);
  }

  const TRInstructionX86& ins = insns.at(1);

  if (ins.insn.type != INSTRUCTION_TYPE_MOV)
  {
    return (-1);
  }

  //the instruction should be something like mov XXX, %eax (in ATT FORMAT)
  // where %eax is the first operand (in INTEL FORMAT) and XXX is the syscall number
  //
  if ( (ins.insn.op1.type != OPERAND_TYPE_REGISTER) || (ins.insn.op1.reg != REG_EAX) )
  {
    return (-1);
  }
  if ( (ins.insn.op2.type == OPERAND_TYPE_IMMEDIATE) )
  {
    size_t syscallNum = ins.insn.op2.immediate;
    if (syscallNum >= SYSCALLTABLE_LINUX_2_6_LEN)
    {
      return (-1);
    }
    curSyscallName = sysCallTable[syscallNum];
    return (0);
  }

  return (-1);
}

int TraceProcessorX86InterruptLinux::processInstruction(const TRInstructionX86& insn, const X86CPUState& cpu)
{
  if (!isInterested(insn))
  {
    return (-1);
  }
  if (cpu.gen.eax >= SYSCALLTABLE_LINUX_2_6_LEN)
  {
    return (-1);
  }
  curSyscallName = sysCallTable[cpu.gen.eax];

  return (0);
}

int TraceProcessorX86InterruptLinux::processInstruction(const string& str, const X86CPUState& cpu)
{
  if (!isInterested(str))
  {
    return (-1);
  }
  if (cpu.gen.eax >= SYSCALLTABLE_LINUX_2_6_LEN)
  {
    return (-1);
  }
  curSyscallName = sysCallTable[cpu.gen.eax];

  return (0);
}

int TraceProcessorX86InterruptLinux::processInstruction(History<string>& strs)
{
  if (strs.size() < 2)
  {
    return (-1);
  }

  if (!isInterested(strs.at(0)))
  {
    return (-1);
  }

  const string& prevS = strs.at(1);

  size_t t = prevS.find("mov");
  if (t == string::npos)
  {
    return(-1);
  }

  //now that we found mov, lets find the $
  size_t t2 = prevS.find('$', t);

  //now that we found the $, we need to find the comma
  size_t t3 = prevS.find(',',t2);

  //now we should just make sure that the next thing is eax
  size_t t4 = prevS.find("%eax", t3);
  //make sure all characters from t3 to t4 are whitespaces
  for (size_t i = t3+1; i < t4; i++)
  {
    if (!isspace(prevS[i]))
    {
      return (-1);
    }
  }

  //if we are here that means everything checked out so lets get
  // the syscall number
  uint32_t syscallNum = 0;
  if(myHexStrToul(syscallNum, prevS.substr(t2+1,t3-t2-1)) != 0)
  {
    return (-1);
  }
  /*
  t4 = str.find("$0x80");

  outs = str.substr(0, t4);
  outs += "SYSCALL:";
  outs += sysCallTable[syscallNum];
  outs += "{";
  outs += str.substr(t4, 5);
  outs += "}";
  outs += str.substr(t4+5);
  */

  if (syscallNum >= SYSCALLTABLE_LINUX_2_6_LEN)
  {
    return (-1);
  }

  curSyscallName = sysCallTable[syscallNum];

  return (0);
}

bool TraceProcessorX86InterruptLinux::isInterested(const string& str)
{
  size_t t = str.find("int");
  if (t == string::npos)
  {
    return (false);
  }
  size_t t2 = str.find("$0x80");
  if (t2 == string::npos)
  {
    return (false);
  }

  if (t2 < t)
  {
    return (false);
  }

  for (size_t i = t+3; i < t2; ++i)
  {
    if (!isspace(str[i]))
    {
      return (false);
    }
  }
  return (true);
}

bool TraceProcessorX86InterruptLinux::isInterested(const TRInstructionX86& insn)
{
  //cout << insn.insn.op1.immediate << " " << 0x80 << endl;
  return ( (insn.insn.type == INSTRUCTION_TYPE_INT) && (insn.insn.op1.type == OPERAND_TYPE_IMMEDIATE) && (insn.insn.op1.immediate == 0x80) && (insn.insn.op2.type == OPERAND_TYPE_NONE) && (insn.insn.op3.type == OPERAND_TYPE_NONE));
}
