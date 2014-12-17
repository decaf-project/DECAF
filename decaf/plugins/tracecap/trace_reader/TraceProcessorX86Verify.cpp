/**
 *  @Author Lok Yan
 *  @date 24 APR 2013
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include "TraceProcessorX86Verify.h"
#include "trace_x86.h"
#include "BAPHelperDefs.h"

using namespace std;

bool TraceProcessorX86Verify::isInterested(const TRInstructionX86& insn)
{
  //need to reintegrate support for older version
  if ( 0//(insn.version != 0x32)
       || (insn.version != 0x33) )
  {
    return (false);
  }

  for (int i = 0; (i < insn.eh.num_operands) && (insn.eh.operand[i].type != TNone) ; i++)
  {
    if (XED_IS_READ_OPERAND(insn.eh.operand[i].access))
    {
      //need to fix this one so that it will use "tainted" for version 32
      //Here we only check for tainted inputs - because if the input is not
      // tainted then the output should not be. If it is then it is an 
      // automatic error
      if (insn.eh.operand[i].tainted_begin)
      {
        return (true);
      }
    }
    //We will also try to handle the case where the registers in the memory
    // operands are also tainted.
    if (insn.eh.operand[i].type == TMemLoc)
    {
      for (int j = 0; (j < MAX_NUM_MEMREGS) && (insn.eh.memregs[i][j].type != TNone); j++)
      {
        if (insn.eh.memregs[i][j].tainted_begin)
        {
          return (true);
        }
      }
    }
  }
  return (false);
}

int TraceProcessorX86Verify::processInstruction(const TRInstructionX86& insn)
{
  //once again need to fix this 
  if ( 0 //insn.version != 0x32)
       || (insn.version != 0x33) )
  {
    return (-1);
  }

  //First lets grab the different parts of the query:
  //the concretized operands
  string cOperands;
  string cEFlags;
  //the goals
  string initGoal("goal:bool = false");
  //the setup operands1
  string sOperands1;
  //the il
  string il;
  //intermediary goal i.e., goal1:bool = OUTPUT OPERAND
  //the setup operands2
  string sOperands2;
  //the il again
  //2nd intermediary goal i.e., goal2:bool = OUTPUT OPERAND
  //the end goal that we will try to validate
  string endGoal("goal:bool");
  string s;
  stringstream ss;

  prBH.concretizeEFlags(cEFlags, insn.eh.eflags, false);

  OperandVal tempOpVal;

  //populate the strings
  for (int i = 0; (i < insn.eh.num_operands) && (insn.eh.operand[i].type != TNone) ; i++)
  {
    if (XED_IS_READ_OPERAND(insn.eh.operand[i].access))
    {
      prBH.concretizeOperand(cOperands, insn.eh, i, prbBitTaint);
      //prBH.setupOperand(sOperands1, insn.eh.operand[i], '1');
      prBH.setupOperand(sOperands1, insn.eh, i, '1');
      sOperands1.append("\n");
      //prBH.setupOperand(sOperands2, insn.eh.operand[i], '2');
      prBH.setupOperand(sOperands2, insn.eh, i, '2');
      sOperands2.append("\n");
    }
    //we have another case where if the operand is a memory operand
    // then we want to concretize the Register RELATIVE operands
    // even if its a write - as long as they are read that is
    else if (insn.eh.operand[i].type == TMemLoc)
    {
      for (int j = 0; (j < MAX_NUM_MEMREGS) && (insn.eh.memregs[i][j].type != TNone); j++)
      {
        if (insn.eh.memregs[i][j].type == TRegister)
        {
          prBH.concretizeOperand(cOperands, insn.eh.memregs[i][j], prbBitTaint, true, "_IDX");
          prBH.setupOperand(sOperands1, insn.eh.memregs[i][j], '1', "_IDX");
          sOperands1.append("\n");
          prBH.setupOperand(sOperands2, insn.eh.memregs[i][j], '2', "_IDX");
          sOperands2.append("\n");
        }
      }
    }
  }

  
  //get the IL  
  prBH.toBAPIL(il, insn);

  //now we iterate through all of the WRITE operands
  for (int i = 0; (i < insn.eh.num_operands) && (insn.eh.operand[i].type != TNone); i++)
  {
    if (XED_IS_WRITE_OPERAND(insn.eh.operand[i].access))
    {
      string opName;
      BAPHelper::operandToString(opName, insn.eh.operand[i]);
      string opSizeString = BYTESIZE_TO_STRING(prBH.operandSize(insn.eh.operand[i]));

      //if byte level then
      if (!prbBitTaint)
      {
        //iterate through each byte of the output operand
        for (int j = 0; j < insn.eh.operand[i].length; j++)
        {
          string tempTaintStr;
          uint64_t tempTaint = (0xFF << (j << 3));
          ss.str(""); //clear the string stream

          prBH.concretizeLiteral(tempTaintStr, tempTaint, insn.eh.operand[i].length);
 //Doesn't look like the child piper works.
/* Still doesn't work. Looks like we will just have to take the SYSTEM approach
          cp.setPath("bap-0.7/utils/topredicate");
          cp.addParam("topredicate");
          cp.addParam("-il");
          cp.addParam("/proc/self/fd/0");
          cp.addParam("-stp-out");
          cp.addParam("last.stp");
          cp.addParam("-post");
          cp.addParam("goal");
          cp.addParam("-solver");
          cp.addParam("z3");
          cp.addParam("-solve");
*/
/*
          cp.setPath("bap-0.7/utils/topredicate");
          cp.addParam("topredicate");
          cp.addParam("-il");
          cp.addParam("test5.il");
          cp.addParam("-stp-out");
          cp.addParam("last5.stp");
          cp.addParam("-post");
          cp.addParam("goal");
          cp.addParam("-solve");
*/           
          cout << "---------------------------------------------------------" << endl;
          cout << "// Query for byte [" << j << "] of " << opName << endl;
          ss << "// Query for byte [" << j << "] of " << opName << endl;
          //first print out the concretized operands
          ss << cOperands << endl;
          //then the eflags
          ss << cEFlags << endl;
          //then print out the init goal
          ss << initGoal << endl; 
          //then print out the first set of operand setups
          ss << sOperands1 << endl;
          //then print out the IL
          ss << il << endl;
          //then print out the intermediary goal
          ss << "goal1:" << opSizeString << " = " << opName << " & " << tempTaintStr << ":" << opSizeString << endl;

          //now print out the second set
          ss << sOperands2 << endl;
          ss << il << endl;
          ss << "goal2:" << opSizeString << " = " << opName << " & " << tempTaintStr << ":" << opSizeString << endl;
         
          ss << "goal:bool = " << "goal1:" << opSizeString << " <> goal2:" << opSizeString << endl;

/*
          cp.spawn();
          cp.writeline(ss.str().c_str());
          cp.sendEOF();

          string x;
          while (cp.readline(x) >= 0)
          {
            cout << x << endl;
          }
*/
          ofstream ofs("last.il",ios_base::trunc | ios_base::out);
          ofs << ss.str();
          ofs.close();

          if (prbVerbose)
          {
            cout << endl; 
            cout << "*** IL ***" << endl;
            cout << ss.str();
     
            cout << endl; 
            cout << "*** BAP ***" << endl;
          }

  
          system("bap-0.7/utils/topredicate -il last.il -stp-out last.stp -post goal -solve");

          if (prbVerbose)
          {
            cout << endl; 
            cout << "*** STP ***" << endl;
            system("cat last.stp");
          }
        
          cout << "---------------------------------------------------------" << endl;
        } 
      }
      else
      {
        //iterate through each bit
                //iterate through each byte of the output operand
        for (int j = 0; j < (insn.eh.operand[i].length * 8); j++)
        {
          string tempTaintStr;
          uint64_t tempTaint = 0x1 << j;
          ss.str(""); //clear the string stream

          prBH.concretizeLiteral(tempTaintStr, tempTaint, insn.eh.operand[i].length);

          cout << "---------------------------------------------------------" << endl;
          cout << "// Query for bit [" << j << "] of " << opName << endl;
          ss << "// Query for bit [" << j << "] of " << opName << endl;
          //first print out the concretized operands
          ss << cOperands << endl;
          //then the eflags
          ss << cEFlags << endl;
          //then print out the init goal
          ss << initGoal << endl; 
          //then print out the first set of operand setups
          ss << sOperands1 << endl;
          //then print out the IL
          ss << il << endl;
          //then print out the intermediary goal
          ss << "goal1:" << opSizeString << " = " << opName << " & " << tempTaintStr << ":" << opSizeString << endl;

          //now print out the second set
          ss << sOperands2 << endl;
          ss << il << endl;
          ss << "goal2:" << opSizeString << " = " << opName << " & " << tempTaintStr << ":" << opSizeString << endl;
         
          ss << "goal:bool = " << "goal1:" << opSizeString << " <> goal2:" << opSizeString << endl;

/*
          cp.spawn();
          cp.writeline(ss.str().c_str());
          cp.sendEOF();

          string x;
          while (cp.readline(x) >= 0)
          {
            cout << x << endl;
          }
*/
          ofstream ofs("last.il",ios_base::trunc | ios_base::out);
          ofs << ss.str();
          ofs.close();

          if (prbVerbose)
          {
            cout << endl; 
            cout << "*** IL ***" << endl;
            cout << ss.str();
    
            cout << endl; 
            cout << "*** BAP ***" << endl;
          }

  
          system("bap-0.7/utils/topredicate -il last.il -stp-out last.stp -post goal -solve");

          if (prbVerbose)
          {
            cout << endl; 
            cout << "*** STP ***" << endl;
            system("cat last.stp");
          }
                  
          cout << "---------------------------------------------------------" << endl;
        } 
      }
    } 
  }
  return (0);
}
