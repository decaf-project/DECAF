/*
 * TraceReaderBinARM.cpp
 *
 *  Created on: Jul 8, 2011
 *      Author: lok
 */
/**
 *  @Author Lok Yan
 */
#include "TraceReaderBinARM.h"
#include <inttypes.h>

#include <iostream>
#include <iomanip>
#include <cstring>
#include <list>
#include <sstream>

using namespace std;

TraceReaderBinARM::TraceReaderBinARM()
{
  bReady = false;
  bCout = false;
  bVerbose = false;
}

TraceReaderBinARM::~TraceReaderBinARM()
{
  ifs.close();
  if (!bCout)
  {
    ofs.close();
  }
}

void TraceReaderBinARM::setHistorySize(size_t newSize, bool override)
{
  if ( (newSize > iHistory.getMaxSize()) || override )
  {
    iHistory.setMaxSize(newSize);
    sHistory.setMaxSize(newSize);
  }
}

int TraceReaderBinARM::run()
{
  while (readNextInstruction() == 0)
  {
    if (bCout)
    {
      cout << sHistory.at(0);
    }
    else
    {
//      string s;
//      entryHeaderToString(s, curInsn.eh);
//      ofs << s;
      ofs << sHistory.at(0);
    }
  }
  return (0);
}

int TraceReaderBinARM::init(string inFileName, string outFileName)
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

  //there is no header in the ARM file as of now

  init_status = ifs.tellg();
  bReady = true;
  return (0);
}



int TraceReaderBinARM::seekTo(streampos loc)
{
  if (!bReady)
  {
    return (-1);
  }
  ifs.seekg(loc);
  //find next entry
  seekNextEntry();
  if (!ifs.good())
  {
    return (-1);
  }
  return (0);
}

int TraceReaderBinARM::reset()
{
  if (!bReady)
  {
    return (-1);
  }
  ifs.seekg(init_status);
  return (0);
}

int TraceReaderBinARM::readNextInstruction()
{
  TRInstructionARM insn;
  int ret = 0;

  //first read the header
  ifs.read((char*)(&(insn.eh.header)), sizeof(ARMInsnHeader));
  //if its the end of the file, then we are done
  if (ifs.eof())
  {
    return (-1);
  }
  if (!ifs.good())
  {
    cerr << "Couln't read entry header. Only [" << ifs.gcount() << "] bytes of [" << sizeof(ARMInsnHeader) << "] read" << endl;
    return (-1);
  }
  //then read the read operands
  if (insn.eh.header.read_opers > TRINSTRUCTION_ARM_MAX_OPERANDS)
  {
    cerr << "Number of read operands [" << insn.eh.header.read_opers << "] exceeds maximum" << endl;
    return (-1);
  }
  if (insn.eh.header.read_opers > 0)
  {
    ifs.read((char*)(insn.eh.read), sizeof(ARMInsnOperand) * insn.eh.header.read_opers);
    //if its the end of the file, then we are done
    if (ifs.eof())
    {
      return (-1);
    }
    if (!ifs.good())
    {
      cerr << "Couln't read read operands. Only [" << ifs.gcount() << "] bytes of [" << (sizeof(ARMInsnOperand)*insn.eh.header.read_opers) << "] read" << endl;
      return (-1);
    }
  }
  //then read the write operands
  if (insn.eh.header.write_opers > TRINSTRUCTION_ARM_MAX_OPERANDS)
  {
    cerr << "Number of write operands [" << insn.eh.header.write_opers << "] exceeds maximum" << endl;
    return (-1);
  }
  if (insn.eh.header.write_opers > 0)
  {
    ifs.read((char*)(insn.eh.write), sizeof(ARMInsnOperand) * insn.eh.header.write_opers);
    //if its the end of the file, then we are done
    if (ifs.eof())
    {
      return (-1);
    }
    if (!ifs.good())
    {
      cerr << "Couln't read write operands. Only [" << ifs.gcount() << "] bytes of [" << (sizeof(ARMInsnOperand)*insn.eh.header.write_opers) << "] read" << endl;
      return (-1);
    }
  }

  string s;

  if (insn.eh.header.deadbeef == 0xDEADBEEF)
  {
    ret = TraceConverterARM::convert(s, insn, false, bVerbose);
  }
  else
  {
    ret = TraceConverterARM::convert(s, insn, true, bVerbose);
  }

  if (ret != 0)
  {
    return (ret);
  }

  iHistory.push(insn);
  sHistory.push(s);

  return 0;
}

int TraceReaderBinARM::seekNextEntry()
{
  /* Entries are no longer fixed in size
  //since entries are of a fixed size we can use that,
  size_t t = ifs.tellg();
  size_t t2 = t % sizeof(EntryHeaderARM);
  if (t2 != 0)
  {
    t += sizeof(EntryHeaderARM) - t2;
  }
  ifs.seekg(t);
  return (t);
  */
  //we should just look for the deadbeef
  return (-1);
}


