/**
 *  @Author Lok Yan
 */
#include "TraceProcessorX86Functions.h"
#include "HelperFunctions.h"

using namespace std;

int TraceProcessorX86Functions::processInstruction(const TRInstructionX86& insn)
{
  if (!isInterested(insn))
  {
    return (-1);
  }

  uint32_t addr = insn.eh.operand[0].value;
  //need special consideration if the type is of JUMP - its relative
  if (insn.eh.operand[0].type == TJump)
  {
    addr += insn.eh.address; // this might overflow
  }

  curFunctionName = sm.find(addr);
  if (curFunctionName.empty())
  {
    return (-1);
  }
  return (0);
}

int TraceProcessorX86Functions::processInstruction(const string& s)
{
  size_t t;
  bool isCall = false;
  //first find the call instruction
  t = getCallPositionInLine(s);
  if (t == string::npos)
  {
    t = getJmpPositionInLine(s);
    if (t == string::npos)
    {
      return (-1);
    }
  }
  else
  {
    isCall = true;
  }

  //find the first non-whitespace character
  size_t i = 0;
  for (i = t + 4; i < s.size() && isspace(s[i]); ++i);

  //now s[i] should not be a space or its > s.size;
  if (i >= s.size())
  {
    return (-1);
  }

  //now that we have found the first none whitespace character lets get the next substring
  for (t = i+1; t < s.size() && !isspace(s[t]); ++t);

  if (t >= s.size())
  {
    return (-1);
  }

  string s2 = s.substr(i, t-i);

  uint32_t addr = 0;

  if (myHexStrToul(addr, s2) == 0)
  {
    //if it was really a hex string then we are good to go
    curFunctionName = sm.find(addr);
    if (curFunctionName.empty())
    {
      return (-1);
    }
  }
  else
  {
    //could not find the call that we are looking for, so lets see
    // if we can just find the address
    //first thing we want to look for is the M@ string
    size_t t2 = s.find("M@", t);
    if (t2 == string::npos)
    {
      t2 = s.find("R@", t);
    }
    if (t2 == string::npos)
    {
      return (-1);
    }
    else
    {
      size_t t3 = s.find('[', t2);
      if (t2 == string::npos)
      {
        return (-1);
      }
      else
      {
        size_t t4 = s.find(']', t3);
        if (t4 == string::npos)
        {
          return (-1);
        }
        else
        {
          //now that we found the address, lets grab it out
          s2 = s.substr(t3+1, t4-t3-1);
          if (myHexStrToul(addr, s2) == 0)
          {
            curFunctionName = sm.find(addr);
            if (curFunctionName.empty())
            {
              return (-1);
            }
          }
          else
          {
            return (-1);
          }
        }
      }
    }
  }
  return (0);
}

size_t TraceProcessorX86Functions::getCallPositionInLine(const string& s)
{
  return(s.find("call"));
}

bool isRetLine(const string& s)
{
  if (s.find("ret") != string::npos)
  {
    return (true);
  }
  if (s.find("exit") != string::npos)
  {
    return (true);
  }
  return (false);
}



size_t TraceProcessorX86Functions::getJmpPositionInLine(const string& s)
{
  size_t ret = 0;
  ret = s.find("jmp");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("je");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jne");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jz");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnz");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jo");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jno");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("js");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jns");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jb");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnb");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jc");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnc");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jae");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnae");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("ja");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnbe");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jl");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnge");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jge");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnl");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jle");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jng");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jg");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnle");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jp");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jpe");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jnp");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jpo");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jcxz");
  if (ret != string::npos)
  {
    return (ret);
  }
  ret = s.find("jecxz");
  if (ret != string::npos)
  {
    return (ret);
  }
  return (ret);
}

bool TraceProcessorX86Functions::isInterested(const TRInstructionX86& insn)
{
  return ((insn.insn.type == INSTRUCTION_TYPE_CALL) || (insn.insn.type == INSTRUCTION_TYPE_JMP) || (insn.insn.type == INSTRUCTION_TYPE_JMPC));
}

bool TraceProcessorX86Functions::isInterested(const string& str)
{
  return ((getCallPositionInLine(str) != string::npos) || (getJmpPositionInLine(str) != string::npos));
}
