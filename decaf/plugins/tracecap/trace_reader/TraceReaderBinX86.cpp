/**
 *  @Author Lok Yan
 */
#include "TraceReaderBinX86.h"
#include <inttypes.h>

#include <string>
#include <iomanip>
#include <cstring>
#include <list>
#include <sstream>
#include <assert.h>

using namespace std;

void TraceReaderBinX86::setHistorySize(size_t newSize, bool override)
{
  if ( (newSize > iHistory.getMaxSize()) || override )
  {
    iHistory.setMaxSize(newSize);
    sHistory.setMaxSize(newSize);
  }
}

int TraceReaderBinX86::run()
{
	while(readNextInstruction() == 0) 
        {
    if (bCout)
    {
      cout << getInsnString();
    }
    else
    {
//      string s;
//      entryHeaderToString(s, curInsn.eh);
//      ofs << s;
      ofs << getInsnString();
    }
  }
  return (0);
}

TraceReaderBinX86::TraceReaderBinX86()
{
  bReady = false;
  bCout = false;
  mbConvertString = true;
  mbVerbose = true;
}

TraceReaderBinX86::~TraceReaderBinX86()
{
  ifs.close();
  if (!bCout)
  {
    ofs.close();
  }
}

int TraceReaderBinX86::init(string inFileName, string outFileName)
{
  if (bReady)
  {
    return (0);
  }

  ifs.open(inFileName.c_str(), ifstream::in | ifstream::binary);
  if (!ifs.good())
  {
    cerr << "Could not open file [" << inFileName << "] for read" << endl;
    return (-1);
  }

  if (outFileName.empty())
  {
    bCout = true;
  }
  else
  {
    ofs.open(outFileName.c_str(), ofstream::out | ofstream::trunc);
    if (!ofs.good())
    {
      cerr << "Could not open file [" << outFileName << "] for write" << endl;
      return (-1);
    }
  }

  //read the first couple of headers
  ifs.read((char*)(&tch), sizeof(tch));
  if (!ifs.good())
  {
    cerr << "Could not read the file header. Only " << ifs.gcount() << " bytes of " << sizeof(tch) << " read" << endl;
    return (-1);
  }

  if (tch.version != 0x33) //if the version is not 51
  {
    cerr << "The version number is incorrect. Looking for 0x33 or 50" << endl;
    return (-1);
  }
 
  ifs.read((char*)(&psr), sizeof(psr));
  if (!ifs.good())
  {
    cerr << "Could not read the process header. Only " << ifs.gcount() << " bytes of " << sizeof(psr) << " read" << endl;
    return (-1);
  }

  if (psr.n_mods > 0)
  {
    if (psr.n_mods > MOD_MAX)
    {
      cerr << "Too many modules [" << psr.n_mods << "]. Can't continue" << endl;
      return (-2);
    }
    ifs.read((char*)(aMRs), sizeof(ModuleRecord) * psr.n_mods);
    if (!ifs.good())
    {
      cerr << "Couldn't read in all the module records. Only " << ifs.gcount() << " bytes of " << sizeof(ModuleRecord) * psr.n_mods << " read" << endl;
      return (-1);
    }
  }

  init_status = ifs.tellg();
  bReady = true;
  return (0);
}



int TraceReaderBinX86::seekTo(streampos loc)
{
  if (!bReady)
  {
    return (-1);
  }
  ifs.seekg(loc);
  //find next entry
  return(seekNextEntry());
}

int TraceReaderBinX86::reset()
{
  if (!bReady)
  {
    return (-1);
  }
  ifs.seekg(init_status);
  return (0);
}

int TraceReaderBinX86::readNextInstruction()
{
  TRInstructionX86 insn;
  int i = 0;
  int j = 0;
  int ret = 0;
  int count = 0;

  if (!bReady)
  {
    return (-1);
  }

  //LOK: Added a line to keep track of the version number
  insn.version = 0x33;//51HU

#ifndef LOK
  ifs.read((char*)(&(insn.eh)), ENTRY_HEADER_FIXED_SIZE);
#else
  ifs.read((char*)(&(insn.eh.address)), sizeof(insn.eh.address));
  ifs.read((char*)(&(insn.eh.tid)), sizeof(insn.eh.tid));
  ifs.read((char*)(&(insn.eh.inst_size)), sizeof(insn.eh.inst_size));
  ifs.read((char*)(&(insn.eh.num_operands)), sizeof(insn.eh.num_operands));
  ifs.read((char*)(&(insn.eh.tp)), sizeof(insn.eh.tp));
  ifs.read((char*)(&(insn.eh.eflags)), sizeof(insn.eh.eflags));
  ifs.read((char*)(&(insn.eh.cc_op)), sizeof(insn.eh.cc_op));
  ifs.read((char*)(&(insn.eh.df)), sizeof(insn.eh.df));
#endif 
  //if its the end of the file, then we are done
  if (ifs.eof())
  {
    return (-1);
  }

  if (!ifs.good())
  {
    cerr << "Couln't read entry header. Only [" << ifs.gcount() << "] bytes of [" << ENTRY_HEADER_FIXED_SIZE << "] read" << endl;
    return (-1);
  }

  //make sure that we don't have an overflow
  if (insn.eh.inst_size > MAX_INSN_BYTES)
  {
    cerr << "Instruction size [" << insn.eh.inst_size << "] is larger than the expected maximum [" << MAX_INSN_BYTES << "]" << endl;
    return (-1);
  }

  ifs.read(insn.eh.rawbytes, insn.eh.inst_size);
  if (!ifs.good())
  {
    cerr << "Couln't read rawbytes. Only [" << ifs.gcount() << "] bytes of [" << insn.eh.inst_size << "] read" << endl;
    return (-1);
  }

  //now read in all the operands
  for(i = 0, j = 0; i < MAX_NUM_OPERANDS && count < insn.eh.num_operands; i++)
  {
    if (readOperand(insn.eh.operand[i]))
    {
      return (-1);
    }
    count++;
    again:
    //if its not memory related then don't need to populate the memory registers
    if(insn.eh.operand[i].type != TMemLoc && insn.eh.operand[i].type != TMemAddress)
    {
      insn.eh.memregs[i][0].type = TNone;
      continue;
    }

    //if this is memory related, then need to process it
    // notice that we starting at the next operand
    for(j = 0; j < MAX_NUM_MEMREGS && count < insn.eh.num_operands; j++)
    {
      //get the next operand
      if (readOperand(insn.eh.memregs[i][j]))
      {
        return (-1);
      }
      count++;

//LOK: I left this here as a test case for the one HU pointed out
// commented it out since we are not testing anymore
/**
if (insn.eh.memregs[i][j].type == TRegister && insn.eh.memregs[i][j].addr == esp_reg)
{ 
  insn.eh.memregs[i][j].value = 0xcfe0; 
//if ESP IS NOT tainted then the resulting taint should be 0x77777777
//if the following line is uncommented, then the taint should be 
// set, which means the resulting taint should be 0xffffffff
//  insn.eh.memregs[i][j].tainted_begin = 0;

}
**/
      //check to see if this operand is a memory-related descriptor - register, like base, limit and etc.
      // if it is then go to the next operand
      if(insn.eh.memregs[i][j].usage > 2)
      {
        continue;
      }
      //this is NOT one of those descriptors, which means its either ESP, Counter, Unknown which would mean
      // its the start of a new operand
      i++;
      //increment i
      //check the loop condition, because this next memcpy
      // is the equivalent of readOperand
      // also because count has been pre-incremented before
      // we check for (count-1) instead of cout
      if (i < MAX_NUM_OPERANDS && (count-1) < insn.eh.num_operands)
      {
        //move the next operand over
        //memcpy(&(eh.operand[i]), &(eh.memregs[i-1][j]), sizeof(OperandVal));
        insn.eh.operand[i] = insn.eh.memregs[i-1][j];
        // so what happens here is we mark the end of the memregs array with TNone
        insn.eh.memregs[i-1][j].type = TNone;
        goto again;
      }
    }

    if(j < MAX_NUM_MEMREGS)
    {
      insn.eh.memregs[i][j].type = TNone;
    }
  }

  if(i < MAX_NUM_OPERANDS)
  {
    insn.eh.operand[i].type = TNone;
  }


  //disassemble the instruction
  if(get_instruction(&(insn.insn), (uint8_t*)(insn.eh.rawbytes), MODE_32) <=0) {
    //if we can't disassemble this instruction for some reason, skip it.
    return 0;
  }

/*
  //now that we are done, lets turn the instruction into a string.
  stringstream ss;
  printInstruction(curInsn, ss);
  strCurInsn = ss.str();
*/

  mCurInsn = insn;

  if (mbConvertString)
  {
    ret = TraceConverterX86::convert(mCurStrInsn, mCurInsn, mbVerbose);
    if (ret != 0)
    {
      return (ret);
    }
    sHistory.push(mCurStrInsn);
  }

  iHistory.push(mCurInsn);
  return 0;
}

int TraceReaderBinX86::readOperand(OperandVal& op)
{
  if (!bReady)
  {
    return (-1);
  }

  int i = 0;
  op.type = TNone;
  op.usage = unknown;

#ifndef LOK
  ifs.read((char*)(&op), OPERAND_VAL_FIXED_SIZE);
#else
  ifs.read((char*)(&op.access), sizeof(op.access));
  ifs.read((char*)(&op.length), sizeof(op.length));
  //ifs.read((char*)(&op.tainted_begin), 6);
  ifs.read((char*)(&op.tainted_begin), sizeof(op.tainted_begin));
  ifs.read((char*)(&op.tainted_end), sizeof(op.tainted_end));
  ifs.read((char*)(&op.addr), sizeof(op.addr));
  ifs.read((char*)(&op.value), sizeof(op.value));
#endif
  if (!ifs.good())
  {
    cerr << "Error reading operand. [" << ifs.gcount() << "] of [" << OPERAND_VAL_FIXED_SIZE << "] bytes read" << endl;
    return(-1);
  }

  ifs.read((char*)(&(op.type)), 1);
  if (!ifs.good())
  {
    cerr << "Error reading operand type" << endl;
    return (-1);
  }

  ifs.read((char*)(&(op.usage)), 1);
  if (!ifs.good())
  {
    cerr << "Error reading operand usage" << endl;
    return (-1);
  }

  if (op.length > MAX_OPERAND_LEN)
  {
    cerr << "Error! operand len [" << op.length << "] is greater than max of [" << MAX_OPERAND_LEN << "]" << endl;
    return (-1);
  }

  for (i = 0; i < op.length; i++)
  {
    //if its not tainted
    if ((op.tainted_begin & (1 << i)) == 0)
    {
      continue;
    }
    //read in the records data structure
#ifndef LOK
    ifs.read((char*)(&(op.records[i])), TAINT_RECORD_FIXED_SIZE);
    if (!ifs.good())
    {
      cerr << "Error reading taint record. [" << ifs.gcount() << "] of [" << TAINT_RECORD_FIXED_SIZE << "] bytes read" << endl;
      return (-1);
    }
#else
    ifs.read((char*)(&(op.records[i].numRecords)), sizeof(op.records[i].numRecords));
    if (!ifs.good())
    {
      cerr << "Error reading taint record. [" << ifs.gcount() << "] of [" << TAINT_RECORD_FIXED_SIZE << "] bytes read" << endl;
      return (-1);
    }

#endif
    //now check to make sure that we have enough room
    if (op.records[i].numRecords > MAX_NUM_TAINTBYTE_RECORDS)
    {
      cerr << "Error, number of taintbyte records [" << op.records[i].numRecords << "] is > max of [" << MAX_NUM_TAINTBYTE_RECORDS << "]" << endl;
      return (-1);
    }

    ifs.read((char*)(&(op.records[i].taintBytes)), sizeof(TaintByteRecord) * op.records[i].numRecords);
    if (!ifs.good())
    {
      cerr << "Error reading taintbyte record. [" << ifs.gcount() << "] of [" << sizeof(TaintByteRecord) * op.records[i].numRecords << "] bytes read" << endl;
      return (-1);
    }
  }

  return (0);
}

int TraceReaderBinX86::seekNextEntry()
{
  return (-1);
}


