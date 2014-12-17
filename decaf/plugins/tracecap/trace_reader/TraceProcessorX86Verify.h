/**
 *  @Author Lok Yan
 * @date 24 APR 2013
 */
#ifndef TRACEPROCESSORX86VERIFY_H
#define TRACEPROCESSORX86VERIFY_H

#include "TraceProcessorX86.h"
#include "BAPHelper.h"

#include "TraceProcessorX86VerifyDefs.h"

class TraceProcessorX86Verify : public TraceProcessorX86
{
public:
  TraceProcessorX86Verify(bool verbose = false, bool bitTaint = false) : prbVerbose(verbose), prbBitTaint(bitTaint), prBH(BAP_TOIL, BAP_HOST_FILE, BAP_HOST_FILE_TEXT_START, BAP_HOST_FILE_TEXT_ADDR, BAP_TEMP_FILE) { prBH.init(); }

  TraceProcessorX86Verify(const std::string& strHostFile, long int textStart, long int textLocation, bool verbose = false, bool bitTaint = false) : prbVerbose(verbose), prbBitTaint(bitTaint), prBH(BAP_TOIL, strHostFile, textStart, textLocation, BAP_TEMP_FILE) { prBH.init(); }

  int processInstruction(const TRInstructionX86& insns);
  int processInstruction(const std::string& strInsns) { return (-1); }
  int processInstruction(History<TRInstructionX86>& insns) { return processInstruction(insns.at(0)); }
  int processInstruction(History<std::string>& strs) { return (processInstruction(strs.at(0))); }
  bool isInterested(const std::string& s) { return (false); }
  bool isInterested(const TRInstructionX86& insn);

  void setVerbose(bool verbose) { prbVerbose = verbose; }
private:
  BAPHelper prBH; 
  bool prbVerbose;
  bool prbBitTaint;
  //ChildPiper prCP; //NOT USED UNTIL WE FIX THE BAP SPAWNING ERROR
};

#endif//TRACEPROCESSORX86VERIFY_H
