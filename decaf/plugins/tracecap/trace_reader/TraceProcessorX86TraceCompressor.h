/*
 * TraceProcessorX86TraceCompressor.h
 *
 *  Created on: Jul 27, 2011
 *      Author: lok
 */

#ifndef TRACEPROCESSORX86TRACECOMPRESSOR_H_
#define TRACEPROCESSORX86TRACECOMPRESSOR_H_

#include "TraceProcessorX86.h"

class TraceProcessorX86TraceCompressor : public TraceProcessorX86
{
public:
  TraceProcessorX86TraceCompressor(size_t historySize):mRepeatCount(0), mLoopInsnCount(0), mCurOffsetInLoopHistory(0), mbProcessingLoop(false) { mHistory.setMaxSize(historySize); mLoopHistory.setMaxSize(historySize); mLoopHistoryLast.setMaxSize(historySize);}
  int processInstruction(const TRInstructionX86& insn);
  int processInstruction(History<TRInstructionX86>& insns) { return (processInstruction(insns.at(0))); }
  int processInstruction(History<std::string>& strInsns) { return (-1); }
  bool isInterested(const std::string& s) { return (false); }
  bool isInterested(const TRInstructionX86& insn) { return (true); }
  const std::string& getResult() { return (mResult); }

  void flush();
private:
  void clear() { mbProcessingLoop = false; mLoopHistory.clear(); mCurOffsetInLoopHistory = 0; mRepeatCount = 0; mLoopInsnCount = 0; }
  std::string mResult;
  History<TRInstructionX86> mHistory;
  History<TRInstructionX86> mLoopHistory;
  History<TRInstructionX86> mLoopHistoryLast;
  unsigned int mRepeatCount;
  unsigned int mLoopInsnCount;
  size_t mCurOffsetInLoopHistory;
  bool mbProcessingLoop;
};

#endif /* TRACEPROCESSORX86TRACECOMPRESSOR_H_ */
