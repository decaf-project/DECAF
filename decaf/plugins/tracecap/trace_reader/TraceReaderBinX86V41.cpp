/**
 *  @Author Lok Yan
 * @Date 18 ARP 2013
 */
#include "TraceReaderBinX86V41.h"
#include <inttypes.h>

#include <string>
#include <iomanip>
#include <cstring>
#include <list>
#include <sstream>

using namespace std;

void TraceReaderBinX86V41::setHistorySize(size_t newSize, bool override)
{
  if ( (newSize > iHistory.getMaxSize()) || override )
  {
    iHistory.setMaxSize(newSize);
    sHistory.setMaxSize(newSize);
  }
}

int TraceReaderBinX86V41::run()
{
  while (readNextInstruction() == 0)
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

TraceReaderBinX86V41::TraceReaderBinX86V41()
{
  bReady = false;
  bCout = false;
  mbConvertString = true;
  mbVerbose = true;
}

TraceReaderBinX86V41::~TraceReaderBinX86V41()
{
  ifs.close();
  if (!bCout)
  {
    ofs.close();
  }
}

int TraceReaderBinX86V41::init(string inFileName, string outFileName)
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
  ifs.read((char*)(&tch), TRACE_HEADER_FIXED_SIZE_V41);
  if (!ifs.good())
  {
    cerr << "Could not read the file header. Only " << ifs.gcount() << " bytes of " << sizeof(tch) << " read" << endl;
    return (-1);
  }

  if (tch.version != 0x29) //if its not version 41 then error
  {
    cerr << "The version number is incorrect. Looking for 0x29 or 41" << endl;
    return (-1);
  }

  for (int i = 0; i < tch.n_procs; i++)
  {
    ifs.read((char*)(&psr), PROC_RECORD_FIXED_SIZE_V41);
    if (!ifs.good())
    {
      cerr << "Could not read the process header. Only " << ifs.gcount() << " bytes of " << sizeof(psr) << " read" << endl;
      return (-1);
    }

    for (int j = 0; j < psr.n_mods; j++)
    {
      if (j >= MOD_MAX)
      {
        cerr << "More modules than I can handle.. overwriting last one" << endl;
        ifs.read((char*)(&aMRs[MOD_MAX-1]), MODULE_RECORD_FIXED_SIZE_V41);
      }
      else
      {
        ifs.read((char*)(&aMRs[j]), MODULE_RECORD_FIXED_SIZE_V41);
      }
      if (!ifs.good())
      {
        cerr << "Couldn't read in all the module records. Only " << ifs.gcount() << " bytes of " << MODULE_RECORD_FIXED_SIZE_V41 * psr.n_mods << " read" << endl;
        return (-1);
      }
    }
  }

  init_status = ifs.tellg();
  bReady = true;
  return (0);
}



int TraceReaderBinX86V41::seekTo(streampos loc)
{
  if (!bReady)
  {
    return (-1);
  }
  ifs.seekg(loc);
  //find next entry
  return(seekNextEntry());
}

int TraceReaderBinX86V41::reset()
{
  if (!bReady)
  {
    return (-1);
  }
  ifs.seekg(init_status);
  return (0);
}

int TraceReaderBinX86V41::readNextInstruction()
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
  insn.version = 0x29;

  ifs.read((char*)(&(insn.ehv41)), ENTRY_HEADER_FIXED_SIZE_V41);
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


  //now read oper - whatever that is
  if (readOperand(insn.ehv41.oper))
  {
    return (-1);
  }

  //now read in all the operands
  for(i = 0, j = 0; i < MAX_NUM_OPERANDS && count < insn.ehv41.num_operands; i++)
  {
    if (readOperand(insn.ehv41.operand[i]))
    {
      return (-1);
    }
    count++;
    again:
    //if its not memory related then don't need to populate the memory registers
    if(insn.ehv41.operand[i].type != TMemLoc && insn.ehv41.operand[i].type != TMemAddress)
    {
      insn.ehv41.memregs[i][0].type = TNone;
      continue;
    }

    //if this is memory related, then need to process it
    // notice that we starting at the next operand
    for(j = 0; j < MAX_NUM_MEMREGS && count < insn.ehv41.num_operands; j++)
    {
      //get the next operand
      if (readOperand(insn.ehv41.memregs[i][j]))
      {
        return (-1);
      }
      count++;

      //check to see if this operand is a memory-related descriptor - register, like base, limit and etc.
      // if it is then go to the next operand
      if(insn.ehv41.memregs[i][j].usage > 2)
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
      if (i < MAX_NUM_OPERANDS && (count-1) < insn.ehv41.num_operands)
      {
        //move the next operand over
        //memcpy(&(eh.operand[i]), &(eh.memregs[i-1][j]), sizeof(OperandVal));
        insn.ehv41.operand[i] = insn.ehv41.memregs[i-1][j];
        // so what happens here is we mark the end of the memregs array with TNone
        insn.ehv41.memregs[i-1][j].type = TNone;
        goto again;
      }
    }

    if(j < MAX_NUM_MEMREGS)
    {
      insn.ehv41.memregs[i][j].type = TNone;
    }
  }

  if(i < MAX_NUM_OPERANDS)
  {
    insn.ehv41.operand[i].type = TNone;
  }

  //now get the instruction rawbytes
  //make sure that we don't have an overflow
  if (insn.ehv41.inst_size > MAX_INSN_BYTES)
  {
    cerr << "Instruction size [" << insn.ehv41.inst_size << "] is larger than the expected maximum [" << MAX_INSN_BYTES << "]" << endl;
    return (-1);
  }

  //sad I had to cast from unsigned char* to char*
  ifs.read((char*)insn.ehv41.rawbytes, insn.ehv41.inst_size);
  if (!ifs.good())
  {
    cerr << "Couln't read rawbytes. Only [" << ifs.gcount() << "] bytes of [" << insn.ehv41.inst_size << "] read" << endl;
    return (-1);
  }

  //disassemble the instruction
  if (get_instruction(&(insn.insn), (uint8_t*)(insn.ehv41.rawbytes), MODE_32) <= 0)
  {
    return (-2);
  }
  else
  {
    //memset(&curInstruction, 0, sizeof(Instruction));
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
    ret = TraceConverterX86V41::convert(mCurStrInsn, mCurInsn, mbVerbose);
    if (ret != 0)
    {
      return (ret);
    }
    sHistory.push(mCurStrInsn);
  }

  iHistory.push(mCurInsn);
  return 0;
}

int TraceReaderBinX86V41::readOperand(OperandValV41& op)
{
  if (!bReady)
  {
    return (-1);
  }

  int i = 0;
  op.type = TNone;
  op.usage = unknown;

  //This is so bad -- looks like the OPERAND_VAL_FIXED_SIZEV41 includes
  // the size of type and usage - whereas it does not in V50... blarg! 
  ifs.read((char*)(&op), OPERAND_VAL_FIXED_SIZE_V41);
  if (!ifs.good())
  {
    cerr << "Error reading operand. [" << ifs.gcount() << "] of [" << OPERAND_VAL_FIXED_SIZE << "] bytes read" << endl;
    return(-1);
  }

  if (op.length > MAX_OPERAND_LEN)
  {
    cerr << "Error! operand len [" << op.length << "] is greater than max of [" << MAX_OPERAND_LEN << "]" << endl;
    return (-1);
  }

  for (i = 0; i < op.length; i++)
  {
    //if its not tainted
    if ((op.tainted & (1 << i)) == 0)
    {
      continue;
    }
    //read in the records data structure
    ifs.read((char*)(&(op.records[i])), TAINT_RECORD_FIXED_SIZE_V41);
    if (!ifs.good())
    {
      cerr << "Error reading taint record. [" << ifs.gcount() << "] of [" << TAINT_RECORD_FIXED_SIZE_V41 << "] bytes read" << endl;
      return (-1);
    }

    //now check to make sure that we have enough room
    if (op.records[i].numRecords > MAX_NUM_TAINTBYTE_RECORDS)
    {
      cerr << "Error, number of taintbyte records [" << op.records[i].numRecords << "] is > max of [" << MAX_NUM_TAINTBYTE_RECORDS << "]" << endl;
      return (-1);
    }

    ifs.read((char*)(&(op.records[i].taintBytes)), TAINT_BYTE_RECORD_FIXED_SIZE_V41 * op.records[i].numRecords);
    if (!ifs.good())
    {
      cerr << "Error reading taintbyte record. [" << ifs.gcount() << "] of [" << sizeof(TaintByteRecord) * op.records[i].numRecords << "] bytes read" << endl;
      return (-1);
    }
  }

  return (0);
}

int TraceReaderBinX86V41::seekNextEntry()
{
  return (-1);
}


