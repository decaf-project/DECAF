/*
 * TraceProcessorX86TraceCompressor.cpp
 *
 *  Created on: Jul 27, 2011
 *      Author: lok
 */

#include "TraceProcessorX86TraceCompressor.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include "TraceConverterX86.h"

using namespace std;

void TraceProcessorX86TraceCompressor::flush()
{
  string str;
  stringstream ss;
  bool bExtra = mbProcessingLoop;
  //first check to see if we are waiting in a loop
  //if we are in the middle of processing a loop - force the loops exit
  if (mbProcessingLoop)
  {
    TRInstructionX86 temp;
    memset(&temp, 0, sizeof(TRInstructionX86));
    temp.eh.address = mLoopHistory.at(mCurOffsetInLoopHistory).eh.address + 1;
    if (get_instruction(&(temp.insn), (uint8_t*)(temp.eh.rawbytes), MODE_32) <= 0)
    {
      cerr << "L:KJSDFLKJSDF" << endl;
    }
    int ret = processInstruction(temp);
    //at this point in time, mResult contains an extra INSTRUCTION, we should remove it
    ss << mResult;
  }

  //get the rest of the history.
  for (size_t t = 0; t < mHistory.size(); t++)
  {
    if ( (bExtra) && (t == (mHistory.size() - 1)) )
    {
      break;
    }
    TraceConverterX86::convert(str, mHistory.at(mHistory.size() - t - 1));
    ss << str;
  }

  mResult = ss.str();

  clear();
  mHistory.clear();
}

int TraceProcessorX86TraceCompressor::processInstruction(const TRInstructionX86& insn)
{
  bool bHasResult = false;
  //before we push it into the history stack, lets see if the history is full
  if ((mLoopInsnCount < mHistory.getMaxSize()) && (mHistory.size() == mHistory.getMaxSize()))
  {
    TraceConverterX86::convert(mResult, mHistory.at(mHistory.size() -1));
    bHasResult = true;
  }
  else
  {
    mResult.clear();
  }

  //now we can push it into the history stack
  mHistory.push(insn);

  //at this point mResult contains the last instruction that we need to output
  // or it is nothing if its part of the loop stuff
  if (mbProcessingLoop)
  {
    if (insn.eh.address == mLoopHistory.at(mCurOffsetInLoopHistory).eh.address)
    {
      //they are the same, so lets see if we are at the end of the history
      if (mCurOffsetInLoopHistory == 0)
      {
        //increment the repeat count
        mRepeatCount++;
        mCurOffsetInLoopHistory = mLoopHistory.size() - 1;
        mLoopHistoryLast.clear();
        //copy the history into the loop history last
        for (size_t t = 0; t < mLoopHistory.size(); t++)
        {
          mLoopHistoryLast.push(mHistory.at(mCurOffsetInLoopHistory - t));
        }
      }
      else
      {
        mCurOffsetInLoopHistory--;
      }
      mLoopInsnCount++;
      if (bHasResult)
      {
        return (1);
      }
      else
      {
        return (2);
      }
    }
    //if repeat count is 0 then just exit as if nothing happened
    else if (mRepeatCount != 0)
    {
      stringstream ss;
      string str;
      //its not in the loop which means we need to print out everything buffered so far
      //first we need to start with mResult
      ss << mResult;

      //then we need to print out ALL of the history that is beyond the loop repeats
      size_t t = mLoopHistory.size() - mCurOffsetInLoopHistory - 1; //because offsets are indices, we need an offset of 1

      //t is the total number of instructions that are NOT part of the loop that we need to handle
      // so t + mLoopInsnCount is the number of instructions in the history that we will ignore
      size_t t3 = 0;
      for (size_t t2 = (t + mLoopInsnCount); t2 < mHistory.size(); t2++)
      {
        t3++;
        TraceConverterX86::convert(str, mHistory.at(mHistory.size() - t3));
        ss << str;
      }

      //now we need to process the LOOPED instructions by first printing out the LOOP itself
      ss << "\n****** LOOP BEGIN *****\n";
      for (size_t t2 = 0; t2 < mLoopHistory.size(); t2++)
      {
        TraceConverterX86::convert(str, mLoopHistory.at(mLoopHistory.size() - t2 - 1));
        ss << str;
      }
      //now print out the number of times it repeats
      ss << "\n**  REPEATED [" << mRepeatCount << "] TIMES  **\n\n";

      //now print out the last state which is the last loop
      for (size_t t2 = 0; t2 < mLoopHistoryLast.size(); t2++)
      {
        TraceConverterX86::convert(str, mLoopHistoryLast.at(mLoopHistoryLast.size() - t2 - 1));
        ss << str;
      }

      //now we can print out the end of this block
      ss << "***** LOOP END *****\n\n";

      //lets just clear the history of all the loop stuff
      mHistory.truncate(t + 1);
      /*
      //now we need to fill in all of the instructions that are NOT part of the repeat
      // We can do this by getting the number of instructions we thought repeated
      // and going into the history to grab them off
      for (size_t t2 = 0; t2 < t1; t2++)
      {
        TraceConverterX86::convert(str, mHistory.at(t1-t2));
        ss << str << '\n';
      }
*/

      //now that we have the full history, get the current instruction as well
      //TraceConverterX86::convert(str, insn);
      //ss << str;
      //update the result
      mResult = ss.str();

      //now that we have the result compiled, lets reset the loop stuff
      clear();
      return (0);
    }
    else
    {
      clear();
    }
    if (bHasResult)
    {
      return (1);
    }
    else
    {
      return (2);
    }
  }

  //check to see if its a rep instruction
  if ((MASK_PREFIX_G1(insn.insn.flags) == MASK_PREFIX_G1(PREFIX_REP))
      || (MASK_PREFIX_G1(insn.insn.flags) == MASK_PREFIX_G1(PREFIX_REPNE))
      )
  {
    //since its a REP instruction, we add it into the loop history
    clear();
    mLoopHistory.push(insn);
    mbProcessingLoop = true;
    mCurOffsetInLoopHistory = mLoopHistory.size() - 1;
    mLoopInsnCount = 1;
    return (2);
  }
  else if (insn.insn.type == INSTRUCTION_TYPE_LOOP)
  {
    //for the loop instruction, we need to first find the loop target
    uint32_t target = insn.eh.address + insn.insn.length + insn.insn.op1.immediate; // I got this formula from libdasm.c
    // where they are processing AM_J for the immediate value
    //now that we have the target lets go backwards and see if we can find the target
    size_t loc = mHistory.size();
    for (size_t t = 0; t < mHistory.size(); t++)
    {
      if (target == mHistory.at(t).eh.address)
      {
        loc = t;
      }
    }

    //loc can't be 0 otherwise it is looping to itself - an infinite loop - or is it?
    if (loc == 0)
    {
       cerr << "LOOPING TO SELF?\n";
    }
    else if (loc < mHistory.size())
    {
      clear();
      for (size_t t = 0; t <= loc; t++)
      {
        mLoopHistory.push(mHistory.at(loc - t));
        mLoopInsnCount++;
      }
      mCurOffsetInLoopHistory = mLoopHistory.size() - 1;
      mbProcessingLoop = true;
      return (2);
    }
  }

  return (1);
}
