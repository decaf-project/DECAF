/**
 * Implementation for BAPHelper
 * @Author Lok Yan
 * @date 22 APR 2013
**/

#include "BAPHelper.h"
#include "BAPHelperDefs.h"

using namespace std;

//the static functions
int BAPHelper::concretizeEFlags(string& str, uint32_t EFLAGS, bool verbose)
{
  for (int i = 0; i < 32; i++)
  {
    if ( (verbose) || BAP_HELPER_IS_STATUS_BIT(i) )
    {
      str.append(EFLAGS_TO_STRING(i)).append(" = ");

      if ( ((EFLAGS >> i) & 0x1) ) //if the ith bit is 1
      {
        str.append("true");
      }
      else
      {
        str.append("false");
      }
    
      str.append("\n");
   }
  }

  return (0);
}

int BAPHelper::concretizeU8(string& str, uint8_t val)
{
  char temp[12];
  snprintf(temp, 11, "0x%02x", val);
  str.append(temp);
  return (0);
}

int BAPHelper::concretizeU16(string& str, uint16_t val)
{
  char temp[12];
  snprintf(temp, 11, "0x%04x", val);
  str.append(temp);
  return (0);
}

int BAPHelper::concretizeU32(string& str, uint32_t val)
{
  char temp[12];
  snprintf(temp, 11, "0x%08x", val);
  str.append(temp);
  return (0);
}

int BAPHelper::concretizeU64(string& str, uint64_t val)
{
  char temp[20];
  snprintf(temp, 19, "0x%016llx", val);
  str.append(temp);
  return (0);
}

int BAPHelper::concretizeLiteral(string& str, uint64_t val, size_t byteLen)
{
  switch (byteLen)
  {
    case (1):
    {
      return (concretizeU8(str, (uint8_t)val));
      break;
    }
    case (2):
    {
      return (concretizeU16(str, (uint16_t)val));
      break;
    }
    case (4):
    {
      return (concretizeU32(str, (uint32_t)val));
      break;
    }
    case (8):
    {
      return (concretizeU64(str, val));
      break;
    }
    default:
    {
      str.append("UNKNOWN");
      return (-1);
      break;
    }
  }  
}

int BAPHelper::concretizeTaint(string& str, const OperandVal& op, bool bBitTaint)
{
  uint64_t temp = 0;
  int ret = 0;
  //check to make sure that temp is long enough for max operand len
  if ( (sizeof(temp) < MAX_OPERAND_LEN) || (op.length > MAX_OPERAND_LEN) )
  {
    return (-1);
  }

  if (bBitTaint)
  {
   /* temp = op.records[0].taintBytes[0].origin;

    if (op.length > 4)
    {
      temp |= ((uint64_t)op.records[0].taintBytes[0].source) << 32;
    }
    */
	  temp = op.tainted_begin;
  }
  else
  {
    for (size_t i = 0; i < op.length; i++)
    {
      //if that bit is tainted
      if ((op.tainted_begin & (1 << i)) != 0)
      {
        //this might be a little confusing, but the idea is to multiple i by 8
        // and shift the 0xFF byte over by that many bit positions
        //then just or it 
        temp |= (0xFF << (i << 3));
      }
    }
  }

  return (concretizeLiteral(str, temp, op.length));
}

int BAPHelper::concretizeValue(string& str, const OperandVal& op)
{
  return (concretizeLiteral(str, op.value, op.length));
}

int BAPHelper::concretizeAddr(string& str, const OperandVal& op)
{
  return (concretizeLiteral(str, op.addr, ADDR_BYTES)); //assumes address is 32-bit
}

uint16_t BAPHelper::operandSize(const OperandVal& op) 
{
  //enforces the requirement that the minimum size is 32 bits
  if (op.type == TRegister)
  {
    if (op.length < 4)
    {
      return (4);
    }
  }

  return (op.length);
}

int BAPHelper::concretizeOperand(string& str, const EntryHeader& eh, unsigned int offset, bool bBitTaint)
{
  if (offset >= MAX_NUM_OPERANDS)
  {
    return (-1);
  }

  const OperandVal& op = eh.operand[offset];
  int i = 0;

  if (concretizeOperand(str, op, bBitTaint) != 0)
  {
    return (-1);
  }
  //NOTE: there is a chance that we will get here if the oeprand type is
  // TNone because the other function returns 0 in that case.
  // But it doesn't matter here because we only handle the TMemLoc case below

  //if it was a memory location that means we also need to concretize the
  // supporting registers
  if (op.type == TMemLoc)
  {
    //because some memory operands depends on certain registers - e.g., pop is based on ESP, we will also need to concretize the ESP or any other register value right now. 
    //TODO: Handle the case when these registers are tainted.
    for (i = 0; (i < MAX_NUM_MEMREGS) && (eh.memregs[offset][i].type != TNone); i++)
    {
      if (eh.memregs[offset][i].type == TRegister)
      {
        //TODO: SUPPORT POINTER TAINTING - right now we have a hack to ignore the taint
        //concretizeOperand(str, eh.memregs[offset][i], bBitTaint, true, "_IDX");
        //HACK: Added a hack here to change the tail to BASE_IDX for the segment
        // registers
        if ( (eh.memregs[offset][i].addr >= es_reg) && (eh.memregs[offset][i].addr <= gs_reg) )
        {
          //concretizeOperand(str, eh.memregs[offset][i], bBitTaint, true, "_BASE_IDX");
          //NOTE: Actually, what we need to do is set the value to 0. The reason for this is because the final memory address that is shown in the log does not actually include the segment base. Basically its only the offset. Here is an example: 
          //(13) 21403c: rorl $0xeb,%fs:0x0(%esi)	I@0x00000000[0x000000eb][1](R) T_begin (0x0) T_end (0x0)	M@0x0000088f[0x00000000][4](RW) T_begin (0xc5c5c5c5) T_end (0xb8b8b8b8)	EFLAGS: 0x00000246 RAW: 0x64c14e00eb OPERAND[0]'s REGISTERS: { R@fs[0x00000048][2](R) T_begin (0x0) T_end (0x0) , R@esi[0x0000088f][4](R) T_begin (0xf0bfcfc5) T_end (0xf0bfcfc5) , M@0x00208048[0x0000ffff][4]() T_begin (0x0) T_end (0x0) , M@0x0020804c[0x00cf9700][4]() T_begin (0x0) T_end (0x0) }
          // Notice how ESI is 0x88f while FS is 0x48 and the final memory address is still 0x88f
          //Now compare this with BAP that uses the absolute address (which makes a lot of sense) - so for now we will have to hack this to zero out the value
          //mem:?u32 with [R_FS_BASE_IDX:u32 + (R_ESI_IDX:u32 + 0:u32), e_little]:u32 =

          //so what we are going to do now is rebuild the segment base address by analyzing the memory accesses
          uint32_t seg_high = 0;
          uint32_t seg_low = 0;
          uint32_t seg_addr = 0xFFFFFFFF;
          uint32_t seg_base = 0;
          for (int j = 0; (j < MAX_NUM_MEMREGS) && (eh.memregs[offset][j].type != TNone); j++)
          {
            if (eh.memregs[offset][j].type == TMemLoc)
            {
              if (seg_addr == 0xFFFFFFFF) //if it wasn't initialized yet
              {
                seg_addr = eh.memregs[offset][j].addr;
                seg_low = eh.memregs[offset][j].value; //assume that this is low
              }
              else
              {
                if (eh.memregs[offset][j].addr < seg_addr) //if its less than
                {
                  seg_high = seg_low;
                  seg_low = eh.memregs[offset][j].value; 
                }
                else
                {
                  seg_high = eh.memregs[offset][j].value;
                }
                //this is the second memory access so we should be done - I hope
                seg_base = (seg_high & 0xFF000000)
                           | ( (seg_high & 0x000000FF) << 16 )
                           | ( (seg_low & 0xFFFF0000) >> 16 );
                break;
              }
            }
          }

          //Here I just made a copy of the operandVal 
          // and then changed the value - this saves me from
          // changing concretizeOperand
          OperandVal pov = eh.memregs[offset][i];
          pov.value = seg_base;
          concretizeOperand(str, pov, bBitTaint, true, "_BASE_IDX");
        }
        else
          concretizeOperand(str, eh.memregs[offset][i], bBitTaint, true, "_IDX");
      }
    }
  }

  return (0);
}

int BAPHelper::concretizeOperand(string& str, const OperandVal& op, bool bBitTaint, bool bIgnoreTaint, const string& tail)
{
  string s;
  string t;
  int ret = 0;

  if (op.type == TRegister)
  {
    if ( (op.addr < 100) || (op.addr >= 140) )
    {
      return (-1);
    }

    size_t regnum = op.addr - 100;

    //so what we want to do here is the following
    //Given a register, e.g. EAX
    //1. define two floating registers R_EAX_O1 and R_EAX_O2
    //2. define one concrete register R_EAX_T representing the taintedness
    //3. define one register R_EAX_C which is the current value of EAX

    
    //then R_EAX as (R_EAX_O1 & R_EAX_T) | (R_EAX_C & ~R_EAX_T)
    // and let the commands go through
    //then R_EAX as (R_EAX_O1 & R_EAX_T) | (R_EAX_C & ~R_EAX_T)

    //1. define two floating registers R_EAX_O1 and R_EAX_O2
    //setup the register name - the left hand side
    //This is not needed, since BAP will handle the definition for us.
    // Just leave it as undeclared
    //s.append(REGNUM_TO_STRING(regnum)).append("_O1:").append(REGNUM_TO_SIZE_STRING(regnum)).append("\n");
    //s.append(REGNUM_TO_STRING(regnum)).append("_O2:").append(REGNUM_TO_SIZE_STRING(regnum)).append("\n"); 

    //2. define one concrete register R_EAX_T representing the taintedness
    s.append(REGNUM_TO_STRING(regnum)).append(tail).append("_T:").append(REGNUM_TO_SIZE_STRING(regnum)); 
    //setup the equals and right hand side
    if (!bIgnoreTaint)
    {
      ret = concretizeTaint(t, op, bBitTaint);
    }
    else
    {
      ret = concretizeLiteral(t, 0ull, op.length);
    }

    if (ret != 0)
    {
      return (ret);
    }
    s.append(" = ").append(t).append(":").append(REGNUM_TO_SIZE_STRING(regnum)).append("\n");

    //3. define one register R_EAX_C which is the current value of EAX
    s.append(REGNUM_TO_STRING(regnum)).append(tail).append("_C:").append(REGNUM_TO_SIZE_STRING(regnum)); 

    t.clear();
    ret = concretizeValue(t, op);
    if (ret != 0)
    {
      return (ret);
    }
    s.append(" = ").append(t).append(":").append(REGNUM_TO_SIZE_STRING(regnum)).append("\n");
   
    str.append(s);
  }
  else if (op.type == TMemLoc)
  {
    //if it is a memory location then we need to set the value
    // in BAP it looks something like:
    //mem:?u32 = mem:?u32 with [R_ESP:u32, e_little]:u32 = T_t:u32
    //but for the concrete value, we will just use a temporary register of sorts
    //instead of having to deal with arrays

    char tempName[32];
    snprintf(tempName, 31, "T_MEM_%u", op.addr);

    s.append(tempName).append(tail).append("_T:").append(BYTESIZE_TO_STRING(op.length)); 
    //setup the equals and right hand side
    if (!bIgnoreTaint)
    {
      ret = concretizeTaint(t, op, bBitTaint);
    }
    else
    {
      ret = concretizeLiteral(t, 0ull, op.length);
    }
    if (ret != 0)
    {
      return (ret);
    }
    s.append(" = ").append(t).append(":").append(BYTESIZE_TO_STRING(op.length)).append("\n");
  
    //now setup the original value
    s.append(tempName).append(tail).append("_C:").append(BYTESIZE_TO_STRING(op.length));
    
    t.clear();
    ret = concretizeValue(t, op);
    if (ret != 0)
    {
      return (ret);
    }
    s.append(" = ").append(t).append(":").append(BYTESIZE_TO_STRING(op.length)).append("\n");
      
    str.append(s);
  }

  return (0);
}

int BAPHelper::operandToString(string& str, const OperandVal& op)
{
  if (op.type == TRegister)
  {
    if ( (op.addr < 100) || (op.addr >= 140) )
    {
      return (-1);
    }

    str.append(REGNUM_TO_FULLSTRING(op.addr - 100));
  }
  else if (op.type == TMemLoc)
  {
    str.append("mem:?");
    str.append(BYTESIZE_TO_STRING(ADDR_BYTES));
    str.append("[");
    concretizeLiteral(str, op.addr, ADDR_BYTES);
    str.append(":").append(BYTESIZE_TO_STRING(ADDR_BYTES));
    str.append(", e_little]:").append(BYTESIZE_TO_STRING(op.length));
  }
}

int BAPHelper::setupOperand(string& str, const EntryHeader& eh, unsigned int offset, char c)
{
  if (offset >= MAX_NUM_OPERANDS)
  {
    return (-1);
  }

  const OperandVal& op = eh.operand[offset];
  int i = 0;

  if (setupOperand(str, op, c) != 0)
  {
    return (-1);
  }
  //NOTE: there is a chance that we will get here if the oeprand type is
  // TNone because the other function returns 0 in that case.
  // But it doesn't matter here because we only handle the TMemLoc case below

  //if it was a memory location that means we also need to concretize the
  // supporting registers
  if (op.type == TMemLoc) 
  {
    //because some memory operands depends on certain registers - e.g., pop is based on ESP, we will also need to concretize the ESP or any other register value right now. 
    //TODO: Handle the case when these registers are tainted.
    for (i = 0; (i < MAX_NUM_MEMREGS) && (eh.memregs[offset][i].type != TNone); i++)
    {
      if (eh.memregs[offset][i].type == TRegister)
      {
        str.append("\n");

        //HACK: Added a hack here to change the tail to BASE_IDX for the segment
        if ( (eh.memregs[offset][i].addr >= es_reg) && (eh.memregs[offset][i].addr <= gs_reg) )
        {
          setupOperand(str, eh.memregs[offset][i], c, "_BASE_IDX");
        }
        else
          setupOperand(str, eh.memregs[offset][i], c, "_IDX");
      }
    }
  }

  return (0);
  
}

int BAPHelper::setupOperand(string& str, const OperandVal& op, char c, const string& tail)
{
  string s;
  string t;
  int ret = 0;

  if (op.type == TRegister)
  {
    if ( (op.addr < 100) || (op.addr >= 140) )
    {
      return (-1);
    }

    size_t regnum = op.addr - 100;

    //Here we are focused on the following part 
    //then R_EAX as (R_EAX_O1 & R_EAX_T) | (R_EAX_C & ~R_EAX_T)
    // and let the commands go through
    //then R_EAX as (R_EAX_O1 & R_EAX_T) | (R_EAX_C & ~R_EAX_T)
    s.append(REGNUM_TO_STRING(regnum)).append(tail).append(":").append(REGNUM_TO_SIZE_STRING(regnum));
    s.append(" = (").append(REGNUM_TO_STRING(regnum)).append(tail).append("_O").append(1,c).append(":").append(REGNUM_TO_SIZE_STRING(regnum));
    s.append(" & ").append(REGNUM_TO_STRING(regnum)).append(tail).append("_T:").append(REGNUM_TO_SIZE_STRING(regnum));
    s.append(") | (").append(REGNUM_TO_STRING(regnum)).append(tail).append("_C:").append(REGNUM_TO_SIZE_STRING(regnum));
    s.append(" & ~").append(REGNUM_TO_STRING(regnum)).append(tail).append("_T:").append(REGNUM_TO_SIZE_STRING(regnum));
    s.append(")");

    str.append(s);
  }
  else if (op.type == TMemLoc)
  {
    //if it is a memory location then we need to set the value
    // in BAP it looks something like:
    //mem:?u32 = mem:?u32 with [R_ESP:u32, e_little]:u32 = T_t:u32
    //but for the concrete value, we will just use a temporary register of sorts
    //instead of having to deal with arrays

    char tempName[32];
    snprintf(tempName, 31, "T_MEM_%u", op.addr);

    s.append("mem:?").append(BYTESIZE_TO_STRING(ADDR_BYTES)).append(" = ");
    s.append("mem:?").append(BYTESIZE_TO_STRING(ADDR_BYTES)).append(" with [");
    
    ret = concretizeAddr(t, op);
    if (ret != 0)
    {
      return (ret);
    }
    s.append(t).append(":").append(BYTESIZE_TO_STRING(ADDR_BYTES)).append(", e_little]:");
    s.append(BYTESIZE_TO_STRING(op.length));

    //now its time to setup the values
    s.append(" = (").append(tempName).append(tail).append("_O").append(1,c).append(":").append(BYTESIZE_TO_STRING(op.length));
    s.append(" & ").append(tempName).append(tail).append("_T:").append(BYTESIZE_TO_STRING(op.length));
    s.append(") | (").append(tempName).append(tail).append("_C:").append(BYTESIZE_TO_STRING(op.length));
    s.append(" & ~").append(tempName).append(tail).append("_T:").append(BYTESIZE_TO_STRING(op.length));
    s.append(")");
   
    str.append(s);
  }

  return (0);
}

BAPHelper::BAPHelper(const string& toilPath, const string& hostFilePath, long int offsetToTextSection, long int offsetToTextSectionAddr, const string& tempFilePath)
{
  prHostFile = NULL;
  prTempFile = NULL;
  prInsnLoc = offsetToTextSection;
  prInsnAddrLoc = offsetToTextSectionAddr;
  prHostFilePath = hostFilePath;
  prToilPath = toilPath;
  prTempFilePath = tempFilePath;
}

int BAPHelper::init()
{
  if (prbInit)
  {
    return (-1);
  }

  prHostFile = fopen(prHostFilePath.c_str(), "r+");
  if (prHostFile == NULL)
  {
    return (-2);
  }

  prTempFile = fopen(prTempFilePath.c_str(), "w");
  if (prTempFile == NULL)
  {
    fclose(prHostFile);
    prHostFile = NULL;
    return (-3);
  }

  prbInit = true;
  return (0);
}

int BAPHelper::cleanup()
{
  if (prHostFile != NULL)
  {
    fflush(prHostFile);
    fclose(prHostFile);
    prHostFile = NULL;
  }
 
  if (prTempFile != NULL)
  {
    fflush(prTempFile);
    fclose(prTempFile);
    prTempFile = NULL;
  } 

  return (0);
}

BAPHelper::~BAPHelper()
{
  cleanup();
}

int BAPHelper::toBAPIL(string& str, const TRInstructionX86& insn)
{
  if (!prbInit)
  {
    return (-1);
  }

  char strAddr[12];
  prLastIL.clear();

  switch (insn.version)
  {
    case (0x29):
    {
      //write the raw instruction out
      fseek(prHostFile, prInsnLoc, SEEK_SET);
      fwrite(insn.ehv41.rawbytes, insn.ehv41.inst_size, 1, prHostFile);
      
      //write the new address
      fseek(prHostFile, prInsnAddrLoc, SEEK_SET);
      fwrite(&insn.ehv41.address, sizeof(uint32_t), 1, prHostFile);
 
      //get the address in hex format
      snprintf(strAddr, 11, "0x%08x", insn.ehv41.address);

      break;
    }
    case (0x32):
    case (0x33):
    {
      //write the raw instruction out
      fseek(prHostFile, prInsnLoc, SEEK_SET);
      fwrite(insn.eh.rawbytes, insn.eh.inst_size, 1, prHostFile);
      
      //write the new address
      fseek(prHostFile, prInsnAddrLoc, SEEK_SET);
      fwrite(&insn.eh.address, sizeof(uint32_t), 1, prHostFile);

      //get the address in hex format
      snprintf(strAddr, 11, "0x%08x", insn.eh.address);

      break;
    }
  }

  //flush the buffer

  fflush(prHostFile);

  //execute BAP
  //run toil
  prCP.setPath(prToilPath);
  prCP.addParam("toil");
  prCP.addParam("-binrange");
  prCP.addParam(prHostFilePath);
  prCP.addParam(strAddr);
  prCP.addParam(strAddr);
  prCP.spawn();

  //cout << strAddr << ":" << endl; 

  //print the output 
  string s;
  string news;
  string t;
  string lval;
  size_t eqloc = string::npos;
  int state = 0;


  while (prCP.readline(s) >= 0)
  {
    //perhaps its better to make sure that the starting position is 0 instead
    //if ( (s.find("addr ") != string::npos) || (s.find("label ") != string::npos) )
    if ( (s.find("addr ") == 0) || (s.find("label ") == 0) )
    {
      prLastIL.append("//"); //make it a comment and move on
      prLastIL.append(s).append("\n");
      continue;
    }

    //if its not a line we need to comment,  process it

    
    news.clear();
    //first we will substitute any register relative memory 
    // accesses with an idx register.
    //handle the case where memory access is relative to a register
    // in this case we should see something like this:
    // addr 0x214027 @asm "mov    %ebx,-0x6(%ebx)"
    // label pc_0x214027
    // mem:?u32 = mem:?u32 with [R_EBX:u32 + -6:u32, e_little]:u32 = R_EBX:u32
    //We want to change this line into
    // mem:?u32 = mem:?u32 with [R_EBX_idx:u32 + -6:u32, e_little]:u32 = R_EBX:u32
    //basically anything within the '[' and ']' that is a register
    // we will replace it with idx.
    for (int i = 0; i < s.length(); i++) //iterate through the string
    {
      switch(state)
      {
        case (0):
        default:
        {
          if (s[i] == '[')
          {
            state = 1;
          }
          news.push_back(s[i]);
          break;
        }
        case (1): //we found the left bracket
        {
          if (s[i] == 'R') //looks like we found an R for register
          {
            state = 2;
          }
          else if (s[i] == ']') //if it ended then go back to state 0
          {
            state = 0;
          }
          news.push_back(s[i]);
          break;
        }
        case (2): //we found a potential register so just iterate until the ':'
        {
          if (s[i] == ':')
          {
            state = 1; //since we didn't see the close bracket yet
            //we will try to find more registers
            news.append("_IDX");
          }
          else if (s[i] == ']') //if it ended then go back to state 0
          {
            state = 0;
          }
          news.push_back(s[i]);
          break;
        }
      }
    }
     
    eqloc = news.find(" =");
    if (eqloc != string::npos)
    { 
      //there are two cases, first is we are the end of the string
      // or if we have " = "
      //note that if the first test is false, then eqloc+2 should exist
      if ( (eqloc == (news.length() - 2))
           || (news[eqloc + 2] == ' ')
         )
      {
        lval = news.substr(0, eqloc);
      }
    }

    //handle the case where we have unknowns
    size_t unknown = news.find("unknown");
    if (unknown != string::npos)
    {
      t = news.substr(0,unknown);
      //at this point we are assuming that the left side of the assignment
      // has already been copied into t
      // so what is left is to write the default value of itself
      t.append(lval);
      t.append("//");
      t.append(s.substr(unknown));
      prLastIL.append(t);
    }
    else
    {
      prLastIL.append(news);
    }
    prLastIL.append("\n");
  }

  str = prLastIL;

  return (0);
}

