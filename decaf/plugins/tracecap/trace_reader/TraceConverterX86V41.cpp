/*
 * TraceConverterX86V41.cpp
 *
 *  Created on: Jun 9, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#include <list>
#include <sstream>
#include <iomanip>
#include "TraceConverterX86V41.h"
#include "HelperFunctions.h"

using namespace std;

//STATIC METHODS
int TraceConverterX86V41::convert(string& str, const TRInstructionX86& i, bool bVerbose)
{
  stringstream ss;
  printInstruction(ss, i, true, bVerbose);
  str = ss.str();
  return (0);
}

int TraceConverterX86V41::convert(TRInstructionX86& insn, const string& str)
{
  //lets try to process the instruction raw bytes first
  int ret = 0;
  size_t s = str.find_first_of(':');
  if (s == string::npos)
  {
    return (-1);
  }

  ret = myHexStrToul(insn.ehv41.address, str.substr(0,s));
  if (ret < 0)
  {
    return (-1);
  }
  //cout << str.substr(0,s) << " ==? " << hex << insn.ehv41.address << endl;
  s = str.rfind("RAW: 0x");
  if (s == string::npos)
  {
    return (-1);
  }

  size_t count = MAX_INSN_BYTES;
  uint8_t* pt = (uint8_t*)insn.ehv41.rawbytes;
  if (myHexStrToBArray(pt, count, str.substr(s+7)) < 0)
  {
    return (-1);
  }
  insn.ehv41.inst_size = count;

  //now lets get the operands one by one
  size_t i = 0;
  size_t t = 0;
  s = 0;
  for (s = str.find_first_of('@', s); (s != string::npos) && (i < MAX_NUM_OPERANDS); s = str.find_first_of('@', t+1))
  {
    if (s == 0)
    {
      return (-1);
    }
    switch (str[s-1])
    {
      case ('R'):
      {
        insn.ehv41.operand[i].type = TRegister;
        break;
      }
      case ('M'):
      {
        insn.ehv41.operand[i].type = TMemLoc;
        break;
      }
      case ('I'):
      {
        insn.ehv41.operand[i].type = TImmediate;
        break;
      }
      case ('N'):
      {
        insn.ehv41.operand[i].type = TNone;
        break;
      }
      case ('J'):
      {
        insn.ehv41.operand[i].type = TJump;
        break;
      }
      case ('F'):
      {
        insn.ehv41.operand[i].type = TFloatRegister;
        break;
      }
      case ('A'):
      {
        insn.ehv41.operand[i].type = TMemAddress;
        break;
      }
    }

    t = str.find_first_of('[',s);
    if (t == string::npos)
    {
      return (-1);
    }

    if (insn.ehv41.operand[i].type != TRegister)
    {
      //get the address
      ret = myHexStrToul(insn.ehv41.operand[i].addr, str.substr(s+1, t - s - 1));
      if (ret < 0)
      {
        return (ret);
      }
    }
    else
    {
      insn.ehv41.operand[i].addr = regStrToAddr(str.substr(s+1, t - s - 1));
      if (insn.ehv41.operand[i].addr == 0xFFFFFFFF)
      {
        return (-1);
      }
    }
    //now get the contents
    s = str.find_first_of('[',t);
    t = str.find_first_of(']',s);
    if (s == string::npos || t == string::npos)
    {
      return (-1);
    }
    ret = myHexStrToul(insn.ehv41.operand[i].value, str.substr(s+1, t - s - 1));
    if (ret < 0)
    {
      return (ret);
    }
    //now get the length
    s = str.find_first_of('[',t);
    t = str.find_first_of(']',s);
    if (s == string::npos || t == string::npos)
    {
      return (-1);
    }
    uint32_t asdf = 0;
    ret = myHexStrToul(asdf, str.substr(s+1, t - s - 1));
    if (ret < 0)
    {
      return (ret);
    }
    if (asdf & ~0xFF)
    {
      return (-1);
    }
    insn.ehv41.operand[i].length = (uint8_t)(asdf & 0xFF);

    //get the access type
    s = str.find_first_of('(',t);
    t = str.find_first_of(')',s);
    if (s == string::npos || t == string::npos)
    {
      return (-1);
    }
    switch (str[s+1])
    {
      case ')':
      {
        insn.ehv41.operand[i].acc = A_Unknown;
        break;
      }
      case 'W':
      {
        insn.ehv41.operand[i].acc = A_W;
        break;
      }
      case 'R':
      {
        if ((t - s) == 2)
        {
          insn.ehv41.operand[i].acc = A_R;
        }
        else if (((t-s) == 3) && (str[t-1] == 'W'))
        {
          insn.ehv41.operand[i].acc = A_RW;
        }
        else if (((t-s) == 4) && (str[s+2] == 'C') && (str[t-1] == 'W'))
        {
          insn.ehv41.operand[i].acc = A_RCW;
        }
        else
        {
          return (-1);
        }
        break;
      }
      case 'C':
      {
        if ((t-s) == 3)
        {
          if (str[t-1] == 'R')
          {
            insn.ehv41.operand[i].acc = A_CR;
          }
          else if (str[t-1] == 'W')
          {
            insn.ehv41.operand[i].acc = A_CW;
          }
        }
        else if (((t-s) == 4) && (str[s+2] == 'R') && (str[t-1] == 'W'))
        {
          insn.ehv41.operand[i].acc = A_CRW;
        }
        else
        {
          return (-1);
        }
        break;
      }
      default:
      {
        return (-1);
      }
    }
    //if (insn.ehv41.address == 0x403b26) cout << str << ":::::" << hex << insn.ehv41.operand[i].addr << endl;
    i++;
  }

  if (i >= MAX_NUM_OPERANDS)
  {
    return (-1);
  }

  insn.ehv41.operand[i].type = TNone;

  return (0);
}



/* Here is the ocaml code
 * (* Priority functions used when sortening operand list *)
let opaccess_priority = function
  | Trace.A_R -> 0
  | Trace.A_CR -> 1
  | Trace.A_CRW -> 2
  | Trace.A_RCW -> 3
  | Trace.A_RW -> 4
  | Trace.A_W -> 5
  | Trace.A_CW -> 6
  | _ -> 7

let optype_priority = function
  | Trace.TImmediate -> 0
  | Trace.TJump -> 1
  | Trace.TRegister -> 2
  | Trace.TFloatRegister -> 3
  | Trace.TMemAddress -> 4
  | Trace.TMemLoc -> 5
  | Trace.TNone -> 6


(* Operand comparison, used to order operands similarly to older traces *)
let operand_compare op1 op2 =
  let p1 = opaccess_priority op1#opaccess in
  let p2 = opaccess_priority op2#opaccess in
  let cmp1 = Pervasives.compare p1 p2 in
  if (cmp1 <> 0) then cmp1
  else (
    let pt1 = optype_priority op1#optype in
    let pt2 = optype_priority op2#optype in
    Pervasives.compare pt1 pt2
  )
 */
//                       {TNone = 0, TRegister, TMemLoc, TImmediate, TJump, TFloatRegister, TMemAddress };
const static int optype_priority[7] = {6, 2, 5, 0, 1, 3, 4};
//                                      {INVALID, RW, R, W, RCW, CW, CRW, CR, LAST}
const static int opaccess_priority[9] = {7, 4, 0, 5, 3, 6, 2, 1, 7};

bool operand_compare (const OperandValV41& op1, const OperandValV41& op2)
{
  int p1 = opaccess_priority[op1.acc];
  int p2 = opaccess_priority[op2.acc];
  if (p1 < p2)
  {
    return (true);
  }
  else if (p2 == p1)
  {
    return (optype_priority[op1.type] < optype_priority[op2.type]);
  }

  return (false);
}

void TraceConverterX86V41::printOperand(const OperandValV41& op, ostream& outs)
{
  if (op.type == TRegister)
  {
    outs << "R@" << regname_map_v41[op.addr-100] << "[0x" << setw(8) << setfill('0') << hex << op.value << "][" << (unsigned int)op.length << "](" << access_map_v41[op.acc] << ") ";
  }
  else
  {
    outs << optype_map_v41[op.type] << "@0x" << setw(8) << setfill('0') << hex << op.addr << "[0x" << setw(8) << setfill('0') << op.value << "][" << (unsigned int)op.length << "](" << access_map_v41[op.acc] << ") ";
  }
  
  outs << "T"<< op.tainted;
}

void TraceConverterX86V41::printInstruction(ostream& outs, const TRInstructionX86& insn, bool bOps, bool bVerbose)
{
  char line[LINE_SIZE];

  outs << hex << insn.ehv41.address << ": ";

  if (get_instruction_string(&(insn.insn), FORMAT_ATT, insn.ehv41.address, line, LINE_SIZE) != 0)
  {
    outs << line << "\t";
  }
  else
  {
    outs << "null\t";
  }

  if (bOps)
  {
    //because we are using the ATT format, we need to do this in reverse order
    //but then the ML script also sorts the operands according to access and then optype

    list<OperandValV41> opList;
    for (int i = 0; (insn.ehv41.operand[i].type != TNone) && (i < MAX_NUM_OPERANDS); i++)
    {
      opList.push_front(insn.ehv41.operand[i]); //this has the effect of doing the reversing
    }
    //now we need to sort
    opList.sort(operand_compare);

    //now we can print it out
    while (!opList.empty())
    {
      printOperand(opList.front(), outs);
      outs << "\t";
      opList.pop_front();
    }
  }

  if (bVerbose) //don't know how to get ESP back yet, so basically we don't do anything except the RAW bytes information here until then
  {
    outs << "EFLAGS: 0x" << setw(8) << setfill('0') << hex << insn.ehv41.eflags << " ";

    outs << "RAW: 0x";
    for (int i = 0; i < insn.ehv41.inst_size; i++)
    {
      uint32_t t = insn.ehv41.rawbytes[i];
      t &= 0xFF;
      outs << setw(2) << setfill('0') << hex << t;
    }
    //outs << "[" << eh.tid << "]";
  }

  outs << endl;
}

uint32_t TraceConverterX86V41::regStrToAddr(const string& str)
{
  //the tracereader prints out regname_map_v41[op.addr-100] for the register name
  //Here is the table
  //const static char regname_map_v41[40][4] = {
  //        {"es"},  {"cs"},  {"ss"},  {"ds"},  {"fs"},  {"gs"},  {""},    {""},    {""},    {""},
  //        {""},    {""},    {""},    {""},    {""},    {""},    {"al"},  {"cl"},  {"dl"},  {"bl"},
  //        {"ah"},  {"ch"},  {"dh"},  {"bh"},  {"ax"},  {"cx"},  {"dx"},  {"bx"},  {"sp"},  {"bp"},
  //        {"si"},  {"di"},  {"eax"}, {"ecx"}, {"edx"}, {"ebx"}, {"esp"}, {"ebp"}, {"esi"}, {"edi"}
  //};
  uint32_t abcd[] = {0,3,1,2};
  if (str.length() == 0)
  {
    //this is kind of strange that there are multiple register values that doesn't have names
    return 106;
  }
  if (str.length() == 2)
  {
    switch (str[1])
    {
      case 'l':
      {
        if (str[0] <= 'd')
        {
          return(abcd[str[0] - 'a'] + 116);
        }
        return (0xFFFFFFFF);
      }
      case 'h':
      {
        if (str[0] <= 'd')
        {
          return(abcd[str[0] - 'a'] + 120);
        }
        return (0xFFFFFFFF);
      }
      case 'x':
      {
        if (str[0] <= 'd')
        {
          return(abcd[str[0] - 'a'] + 124);
        }
        return (0xFFFFFFFF);
      }
      case 'p':
      {
        if (str[0] == 's') return (128);
        if (str[0] == 'b') return (129);
        return (0xFFFFFFFF);
      }
      case 'i':
      {
        if (str[0] == 's') return (130);
        if (str[0] == 'd') return (131);
        return (0xFFFFFFFF);
      }
      case 's':
      {
        if (str[0] == 'e') return (100);
        if (str[0] == 'c') return (101);
        if (str[0] == 's') return (102);
        if (str[0] == 'd') return (103);
        if (str[0] == 'f') return (104);
        if (str[0] == 'g') return (105);
        return (0xFFFFFFFF);
      }
      default:
      {
        return (0xFFFFFFFF);
      }
    }
  }
  if (str.length() == 3)
  {
    switch (str[1])
    {
      case 'a':
      {
        return (132);
      }
      case 'c':
      {
        return (133);
      }
      case 'd':
      {
        if (str[2] == 'x') return (134);
        if (str[2] == 'i') return (139);
        return (0xFFFFFFFF);
      }
      case 'b':
      {
        if (str[2] == 'x') return (135);
        if (str[2] == 'p') return (137);
        return (0xFFFFFFFF);
      }
      case 's':
      {
        if (str[2] == 'p') return (136);
        if (str[2] == 'i') return (138);
        return (0xFFFFFFFF);
      }
      default:
      {
        return (0xFFFFFFFF);
      }
    }
  }
  return (0xFFFFFFFF);

}

//REGULAR METHODS
int TraceConverterX86V41::convert(const TRInstructionX86& i)
{
  mInsn = i;
  return (TraceConverterX86V41::convert(mStrInsn, mInsn, bVerbose));
}

int TraceConverterX86V41::convert(const string& str)
{
  mStrInsn = str;
  return (TraceConverterX86V41::convert(mInsn, mStrInsn));
}

int TraceConverterX86V41::convert(const EntryHeaderV41& eh)
{
  mInsn.ehv41 = eh;

  //disassemble the instruction
  if (get_instruction(&(mInsn.insn), (uint8_t*)(mInsn.ehv41.rawbytes), MODE_32) <= 0)
  {
    return (-1);
  }

  return (TraceConverterX86V41::convert(mStrInsn, mInsn));
}

int TraceConverterX86V41::convert(const EntryHeaderV41* peh)
{
  if (peh == NULL)
  {
    return (-1);
  }
  mInsn.ehv41 = *(peh);
  //disassemble the instruction
  if (get_instruction(&(mInsn.insn), (uint8_t*)(mInsn.ehv41.rawbytes), MODE_32) <= 0)
  {
    return (-1);
  }

  return (TraceConverterX86V41::convert(mStrInsn, mInsn));
}




