/**
 *  @Author Lok Yan
 */
#ifndef TRACE_READER_H
#define TRACE_READER_H

#include <string>
#include <fstream>

//enum insn_t { INSN_JMP, INSN_CALL, INSN_RET, INSN_INT };

class TraceReader
{
public:
  virtual int init(std::string inFileName, std::string outFileName) = 0;
  virtual int run() = 0;
  virtual int seekTo(std::ifstream::streampos loc) =  0;
  virtual int reset() = 0;
  virtual std::ifstream::streampos getFilePos() = 0;
  virtual int readNextInstruction() = 0;
  //virtual insn_t getInstructionType() = 0;
};
#endif//TRACE_READER_H
