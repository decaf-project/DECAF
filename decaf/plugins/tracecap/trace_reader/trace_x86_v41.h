#ifndef TRACE_X86_V41_H
#define TRACE_X86_V41_H

#include <inttypes.h>
#include "trace_x86.h"

#if 0 //Same as in V50
#define MAGIC_NUMBER 0xFFFFFFFF
#endif

/* Taint propagation definitions */
#define TP_NONE 0           // No taint propagation
#define TP_SRC 1            // Taint propagated from SRC to DST
#define TP_CJMP 2           // Cjmp using tainted EFLAG
#define TP_MEMREAD_INDEX 3  // Memory read with tainted index
#define TP_MEMWRITE_INDEX 4 // Memory write with tainted index
#define TP_REP_COUNTER 5    // Instruction with REP prefix and tainted counter
#define TP_SYSENTER 6       // Sysenter

#if 0 //same as in V50
/* Trace format definitions */
#define MAX_NUM_OPERANDS 30 // FNSAVE has a memory operand of 108 bytes 
#define MAX_NUM_MEMREGS 5  /* Max number of memregs per memory operand */
#define MAX_NUM_TAINTBYTE_RECORDS 3
#define MAX_STRING_LEN 32
#define MAX_OPERAND_LEN 8 /* Max length of an operand in bytes */
#define MAX_INSN_BYTES 15 /* Maximum number of bytes in a x86 instruction */

//these aren't in V50, but shouldn't be needed --- right?
#define BLOCK(h) (reinterpret_cast<char*>(&(h)))
#define CHAR 1
#define INT16 2
#define INT32 4
#define INT64 8

enum OpType { TNone = 0, TRegister, TMemLoc, TImmediate, TJump, TFloatRegister, TMemAddress };

enum OpUsage { unknown = 0, esp, counter, membase, memindex, memsegment,
  memsegent0, memsegent1 };

#endif

#define TAINT_BYTE_RECORD_FIXED_SIZE_V41 12

struct TaintByteRecordV41{
  uint32_t source;              // Tainted data source (network,keyboard...)
  uint32_t origin;              // Identifies a network flow
  uint32_t offset;              // Offset in tainted data buffer (network)
};

#define TAINT_RECORD_FIXED_SIZE_V41 4

struct TaintRecordV41{
  uint16_t taint_propag;
  uint16_t numRecords;          // How many TaintByteRecord currently used
  TaintByteRecordV41 taintBytes[MAX_NUM_TAINTBYTE_RECORDS];
};

#define OPERAND_VAL_FIXED_SIZE_V41 28
#define OPERAND_VAL_ENUMS_REAL_SIZE_V41 2

enum OpAccessV41 {A_Unknown = 0, A_RW, A_R, A_W, A_RCW, A_CW, A_CRW, A_CR} ;

struct OperandValV41{
  enum OpType type;
  enum OpUsage usage;
  uint32_t length;
  uint32_t addr;
  uint32_t value;
  uint64_t tainted;
  TaintRecordV41 records[MAX_OPERAND_LEN];
  enum OpAccessV41 acc;
};

#define ENTRY_HEADER_FIXED_SIZE_V41 44

/* Entry header description
  address:       Address where instruction is loaded in memory
  tid:           Thread identifier
  inst_size:     Number of bytes in x86 instruction
  num_operands:  Number of operands (includes all except ESP)
  tp:            Taint propagation value. See above.
  eflags:        Value of the EFLAGS register
  cc_op:         Determines operation performed by QEMU on CC_SRC,CC_DST.
                   ONLY REQUIRES 8-bit
  df:            Direction flag. Has to be -1 (x86_df=1) or 1 (x86_df = 0)
                    COULD BE DERIVED FROM eflags
  operand[]:     Operands accessed by instruction
  memregs[][idx]:   Operands used for indirect addressing
    idx == 0 -> Segment register
    idx == 1 -> Base register
    idx == 2 -> Index register
    idx == 3 -> Segent0
    idx == 4 -> Segent1
  rawybytes[]:   Rawbytes of the x86 instruction
*/
struct EntryHeaderV41 {
  uint32_t address;
  uint32_t tid;
  uint16_t inst_size;
  uint8_t num_operands;
  uint8_t tp;
  uint32_t eflags;
  uint32_t cc_op;
  uint32_t df;

  uint32_t hflags;
  uint32_t aldt;
  uint32_t agdt;
  uint32_t atr;
  uint32_t aidt;
  OperandValV41 oper;
  OperandValV41 operand[MAX_NUM_OPERANDS];
  OperandValV41 memregs[MAX_NUM_MEMREGS][MAX_NUM_MEMREGS]; // 1-dim MAX_NUM_OPERANDS -> MAX_NUM_MEMREGS
  unsigned char rawbytes[MAX_INSN_BYTES];
};

// Same as in V50
#define PROC_RECORD_FIXED_SIZE_V41 40
struct ProcRecordV41{
  char name[MAX_STRING_LEN];
  uint32_t pid;
  uint32_t n_mods;
  uint32_t ldt_base;
};

#define MODULE_RECORD_FIXED_SIZE_V41 40
struct ModuleRecordV41{
  char name[MAX_STRING_LEN];
  uint32_t base;
  uint32_t size;
};

#define TRACE_HEADER_FIXED_SIZE_V41 12
struct TraceHeaderV41 {
  uint32_t magicnumber;
  uint32_t version;
  uint32_t n_procs;
  uint32_t gdt_base;
  uint32_t idt_base;
};

#endif // TRACE_X86_V41_H

