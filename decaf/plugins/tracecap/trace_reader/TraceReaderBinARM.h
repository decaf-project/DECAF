/**
 *  @Author Lok Yan
 */
#ifndef TRACE_READER_BIN_ARM_H
#define TRACE_READER_BIN_ARM_H

#include <string>
#include "TraceReader.h"
#include "History.h"
#include "TRInstructionARM.h"
#include "TraceConverterARM.h"

class TraceReaderBinARM : public TraceReader
{
public:
  TraceReaderBinARM();
  int run();
  int init(std::string inFileName, std::string outFileName);
  int seekTo(std::ifstream::streampos loc);
  //sets the reader to the first instruction (not the beginning of file)
  int reset();
  //reads in the next instruction
  // and automatically turns it into a string
  int readNextInstruction();

  std::ifstream::streampos getFilePos() {return (ifs.tellg());}

  const TRInstructionARM& getCurInstruction() { return (iHistory.at(0)); }
  const std::string& getInsnString() { return (sHistory.at(0)); }
  void setHistorySize(size_t newSize, bool override = false);
  size_t getHistorySize() { return (iHistory.getMaxSize()); }
  History<std::string>& getStringHistory() { return (sHistory); }
  History<TRInstructionARM>& getInstructionHistory() { return (iHistory); }

  void setVerbose(bool v) { bVerbose = v; }
  virtual
  ~TraceReaderBinARM();
protected:
  //helper for finding the next valid entry from the current file location
  int seekNextEntry();

  bool bReady;
  bool bCout;
  bool bVerbose;

  std::ofstream ofs;
  std::ifstream ifs;

  std::ifstream::streampos init_status;

  int historySize;
  History<TRInstructionARM> iHistory;
  History<std::string> sHistory;
};

#endif//TRACE_READER_BIN_ARM_H
