/**
 *  @Author Lok Yan
 */
#ifndef TRINSTRUCTIONX86_H
#define TRINSTRUCTIONX86_H

#include "trace_x86.h"
#include "trace_x86_v41.h"
#include "libdasm.h"
//#include <cstring>

struct TRInstructionX86
{
  uint32_t version;
  union
  {
    EntryHeader eh;
    EntryHeaderV41 ehv41;
  };
  INSTRUCTION insn;
// TRInstructionX86() { memset(&eh, 0, sizeof(EntryHeader)); memset(&insn, 0, sizeof(INSTRUCTION)); }
};

#endif//TRINSTRUCTIONX86_H
