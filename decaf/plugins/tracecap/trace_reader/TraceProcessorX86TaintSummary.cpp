/**
 *  @Author Lok Yan
 *  @date 24 APR 2013
 */

#include <stdio.h>
#include "TraceProcessorX86TaintSummary.h"
#include "TraceConverterX86.h"
#include "trace_x86.h"

using namespace std;

bool TraceProcessorX86TaintSummary::isSameRegister(uint32_t regnum1, uint32_t regnum2)
{
  switch(regnum1)
  {
    case (eax_reg):
    case (al_reg):
    case (ah_reg):
    case (ax_reg):
    {
      return ( (regnum2 == eax_reg) || (regnum2 == al_reg) || (regnum2 == ax_reg) || (regnum2 == ah_reg) );
      break;
    }
    case (ecx_reg):
    case (cl_reg):
    case (ch_reg):
    case (cx_reg):
    {
      return ( (regnum2 == ecx_reg) || (regnum2 == cl_reg) || (regnum2 == cx_reg) || (regnum2 == ch_reg) );
      break;
    }
    case (edx_reg):
    case (dl_reg):
    case (dh_reg):
    case (dx_reg):
    {
      return ( (regnum2 == edx_reg) || (regnum2 == dl_reg) || (regnum2 == dx_reg) || (regnum2 == dh_reg) );
      break;
    }
    case (ebx_reg):
    case (bl_reg):
    case (bh_reg):
    case (bx_reg):
    {
      return ( (regnum2 == ebx_reg) || (regnum2 == bl_reg) || (regnum2 == bx_reg) || (regnum2 == bh_reg) );
      break;
    }
    case (esp_reg):
    case (sp_reg):
    {
      return ( (regnum2 == esp_reg) || (regnum2 == sp_reg) );
      break;
    }
    case (ebp_reg):
    case (bp_reg):
    {
      return ( (regnum2 == ebp_reg) || (regnum2 == bp_reg) );
      break;
    }
    case (esi_reg):
    case (si_reg):
    {
      return ( (regnum2 == esi_reg) || (regnum2 == si_reg) );
      break;
    }
    case (edi_reg):
    case (di_reg):
    {
      return ( (regnum2 == edi_reg) || (regnum2 == di_reg) );
      break;
    }

    default:
    {
      return (regnum1 == regnum2);
    }
  }
  return (false);
}

static uint32_t taintedToTaint(uint32_t tainted)
{
  uint32_t temp = 0;

  for (size_t i = 0; i < 4; i++)
  {
    //if that bit is tainted
    if ((tainted & (1 << i)) != 0)
    {
      //this might be a little confusing, but the idea is to multiple i by 8
      // and shift the 0xFF byte over by that many bit positions
      //then just or it 
      temp |= (0xFF << (i << 3));
    }
  }

  return (temp);
}


int TraceProcessorX86TaintSummary::updateQueue(uint32_t addr, const OperandVal& op, bool bShouldAdd)
{
  TaintEntry* te = NULL;
  bool bIsNew = true;
 
  list<TaintEntry*>::iterator it;
  for (it = prQ.begin(); it != prQ.end(); it++)
  {
    //for each entry in the queue right now, update the nearest taint value
    te = *it;
    if (te == NULL)
    {
      return (-1); //should never happen
    }
  
    if (prNumInsns > te->insnNum)
    {
      if (op.type == TRegister)
      // if its a register, then we have to be careful about what to print
      // this is extremely tricky because we have to deal with low to high and
      // high to low - e.g. eax was tainted previously, and now what happens
      // when we see ax? Do we replace the higher values? it is very tricky
      {
        if (isSameRegister(op.addr, te->op.addr))
        {
          if (prbBitTaint)
          {
            if (op.tainted_end) //Hu
            {
              te->outputTaint = op.tainted_end;//op.records[0].taintBytes[0].origin;
            }
          } 
          else
          {
            te->outputTaint = taintedToTaint(op.tainted_end);//Hu
          }
          te->bReady = true;
          break;
        }
      }
      else if ( (op.type == te->op.type) && (op.addr == te->op.addr))
      {
        //if the type and addr are the same (which means the same location
        // then update the taint information
        if (prbBitTaint)
        {
          if (op.tainted_end)
          {
            te->outputTaint = op.tainted_end; //Hu//op.records[0].taintBytes[0].origin;
          }
        }
        else
        {
          te->outputTaint = taintedToTaint(op.tainted_end);//Hu
        }
        //once we updated the entry, now its time to quit
        //The question to ask is whether this is the correct logic -
        // it should be because we have a nice ordering of the operand accesses
        // as long as we fix the case where an operand is both read and write
        // which is handled with the EntryNum test
        te->bReady = true;
        break;
      }
    } 
  }

  //if any of the operands is tainted then all operands should be added
  if (bShouldAdd)
  {
    //prior to adding the new entry, we must first see if an entry has already been made
    // for the current register. For example in xor %eax, %eax, %eax appears two times
    // but should only have one single entry
    list<TaintEntry*>::reverse_iterator rit;
    for (rit = prQ.rbegin(); rit != prQ.rend(); rit++)
    {
      te = *rit;
      if (te == NULL)
      {
        //should not be here
        return (-1);
      }

      if (prNumInsns != te->insnNum)
      {
        break; //if they are not the same number, then just break nothing else to do
      }
      if (op.type == TRegister)
      {
        if (isSameRegister(op.addr, te->op.addr))
        {
          bIsNew = false;
          break;
        }
      }
      else if ( (op.type == te->op.type) && (op.addr == te->op.addr))
      {
        bIsNew = false;
        break;
      }
    }

    if (bIsNew)
    {
      te = new(TaintEntry);
      if (te == NULL)
      {
        return (-1);
      }
      te->insn_addr = addr;
      if (prbBitTaint)
      {
        te->inputTaint =op.tainted_end;// op.records[0].taintBytes[0].origin;
      }
      else
      {
        te->inputTaint = taintedToTaint(op.tainted_end);
      }
      te->outputTaint = 0;
      te->insnNum = prNumInsns;
      te->op = op;
      te->bReady = false;

      prQ.push_back(te);
    }
  }

  return (0);
}

int TraceProcessorX86TaintSummary::processInstruction(const TRInstructionX86& insn)
{
  bool bShouldAdd = false;
  if (insn.version == 0x33) //if this is version 33 
  // then we just read out the taint values from the data structure
  {
    //clear the previous result
    prResult.clear();

    prNumInsns++; 

    //this one is simpler - just go through the list of
    //operands and print out the taint values
    for (int i = 0; (i < insn.eh.num_operands) && (insn.eh.operand[i].type != TNone); i++)
    {
      char tempStr[128];
      string s;
      if ( insn.eh.operand[i].tainted_begin || insn.eh.operand[i].tainted_end )
      {
        TraceConverterX86::operandToString(s, insn.eh.operand[i]);
        snprintf(tempStr, 127, "TAINT: (%u) %08x : %s 0x%08llx -> 0x%08llx ?\n", prNumInsns, insn.eh.address, s.c_str(), insn.eh.operand[i].tainted_begin, insn.eh.operand[i].tainted_end);
        prResult.append(tempStr);
      }
    }

    return (0);
  }

  if (insn.version != 0x32)
  {
    return (-1);
  }

  //clear the previous result
  prResult.clear();

  prNumInsns++; 

  //what we will do is, first go through all of the operands
  // and determine whether the information can complete any 
  // of the entries that are already inside the queue

  for (int i = 0; (i < insn.eh.num_operands) && (insn.eh.operand[i].type != TNone); i++)
  {
    if (insn.eh.operand[i].tainted_end)
    {
      bShouldAdd = true;
      break;
    }
  }

  for (int i = 0; (i < insn.eh.num_operands) && (insn.eh.operand[i].type != TNone); i++)
  {
    //for each operand, update the queue
    updateQueue(insn.eh.address, insn.eh.operand[i], bShouldAdd);
  }

  //now that all of the operands have been updated, lets just see
  // if there is anything to print out
  list<TaintEntry*>::iterator it = prQ.begin();
  TaintEntry* te = NULL;
  char tempStr[128];
  string s;

  //originally this was done separate from setting the bReady to true so
  // I can buffer things so that the taint values are printed
  // in the same order as the instructions
  //see below
  while (it != prQ.end())
  {
    te = *it;

    if ( te == NULL)
    {
      cerr << "WHAT IS GOING ON?" << endl;
    }
   
    if ( te->bReady )
    {
            //if the next entry is ready then we will process the taint information
      TraceConverterX86::operandToString(s, te->op);
      snprintf(tempStr, 127, "TAINT: (%u) %08x : %s 0x%08llx -> 0x%08llx ?\n", te->insnNum, te->insn_addr, s.c_str(), te->inputTaint, te->outputTaint);
      prResult.append(tempStr);
      it = prQ.erase(it);
      delete(te);
      te = NULL;
    }
    else
    {
      //if I wanted to print things in order then we would just stop at the first 
      //entry that is not ready
      it++;
    }
  }
 
  return (0);
}
