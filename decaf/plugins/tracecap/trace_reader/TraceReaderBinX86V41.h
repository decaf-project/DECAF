/**
 *  @Author Lok Yan
 * @date 18 APR 2013
 */
#ifndef TRACE_READER_BIN_X86_V41_H
#define TRACE_READER_BIN_X86_V41_H

#include <iostream>
#include <fstream>
#include <string>

//#include "trace_x86.h"
//#include "libdasm/libdasm.h"
#include "History.h"
//#include "libdasm.h"
#include "TraceReader.h"
#include "TRInstructionX86.h"
#include "TraceConverterX86V41.h"


class TraceReaderBinX86V41 : public TraceReader
{
public:
  //Constructor
  TraceReaderBinX86V41();
  //initializes the reader by opening the files and reading the trace headers
  int init(std::string inFileName, std::string outFileName);

  int run();
  //go to a location in the file, it will try to find the
  // next header automatically if the magic number is available, otherwise,
  // loc must be the exact location of the start of an instruction
  int seekTo(std::ifstream::streampos loc);
  //sets the reader to the first instruction (not the beginning of file)
  int reset();
  //reads in the next instruction
  // and automatically turns it into a string
  int readNextInstruction();

  void disableStringConversion() { mbConvertString = false; }
  void enableStringConversion() { mbConvertString = true; }
  //insn_t getInstructionType();

  //a bunch of getters
  const TRInstructionX86& getCurInstruction() {return (mCurInsn);}
  const TraceHeader& getTraceHeader() { return (tch); }
  const ProcessRecord& getProcessRecord() { return (psr); }
  const std::string& getInsnString() { if (!mbConvertString) {TraceConverterX86V41::convert(mCurStrInsn, mCurInsn, mbVerbose);} return (mCurStrInsn); }
  void setHistorySize(size_t newSize, bool override = false);
  size_t getHistorySize() { return (iHistory.getMaxSize()); }
  History<std::string>& getStringHistory() { return (sHistory); }
  History<TRInstructionX86>& getInstructionHistory() { return (iHistory); }

  //a setter
  void setVerbose(bool newVerb) { mbVerbose = newVerb; }

  ~TraceReaderBinX86V41();

  //can be used to turn an instruction (entry header) into a string
  int entryHeaderToString(std::string& str, const EntryHeaderV41& eh);

  std::ifstream::streampos getFilePos() { if (ifs.good()) return (ifs.tellg()); else return (0); }

private:
  //helper for reading the next operand
  int readOperand(OperandValV41& op);
  //reads the file header
  int readHeader();
  //helper for finding the next valid entry from the current file location
  int seekNextEntry();

  bool bReady;
  bool bCout;

  bool mbConvertString;
  bool mbVerbose;

  std::ofstream ofs;
  std::ifstream ifs;

  TraceHeader tch;
  ProcessRecord psr;
  ModuleRecord aMRs[MOD_MAX];

  std::ifstream::streampos init_status;

  int historySize;
  History<TRInstructionX86> iHistory;
  History<std::string> sHistory;

  TRInstructionX86 mCurInsn;
  std::string mCurStrInsn;
};

#endif//TRACE_READER_BIN_X86_V41_H
