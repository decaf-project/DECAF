/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
*/
#ifndef _TRACE_H_
#define _TRACE_H_

#include <inttypes.h>
#include "disasm.h"
/* AWH - Conflict between BFD and QEMU's fpu/softfloat.h */
#ifdef INLINE
#undef INLINE
#endif /* INLINE */
//#include "DECAF_lib.h"
#include "DECAF_main.h" // AWH



/* Size of buffer to store instructions */
#define FILEBUFSIZE 104857600

/* Trace header values */
#define VERSION_NUMBER 51 /*bit wise tainting*/
#define MAGIC_NUMBER 0xFFFFFFFF

/* Taint origins */
#define TAINT_SOURCE_NIC_IN 0
#define TAINT_SOURCE_KEYBOARD_IN 1
#define TAINT_SOURCE_FILE_IN 2
#define TAINT_SOURCE_NETWORK_OUT 3
#define TAINT_SOURCE_API_TIME_IN 4
#define TAINT_SOURCE_API_FILE_IN 5
#define TAINT_SOURCE_API_REGISTRY_IN 6
#define TAINT_SOURCE_API_HOSTNAME_IN 7
#define TAINT_SOURCE_API_FILE_INFO_IN 8
#define TAINT_SOURCE_API_SOCK_INFO_IN 9
#define TAINT_SOURCE_API_STR_IN 10
#define TAINT_SOURCE_API_SYS_IN 11
#define TAINT_SOURCE_HOOKAPI 12
#define TAINT_SOURCE_LOOP_IV 13
#define TAINT_SOURCE_MODULE 14

/* Starting origin for network connections */
#define TAINT_ORIGIN_START_TCP_NIC_IN 10000
#define TAINT_ORIGIN_START_UDP_NIC_IN 11000
#define TAINT_ORIGIN_MODULE           20000

/* Taint propagation definitions */
#define TP_NONE 0           // No taint propagation
#define TP_SRC 1            // Taint propagated from SRC to DST
#define TP_CJMP 2           // Cjmp using tainted EFLAG
#define TP_MEMREAD_INDEX 3  // Memory read with tainted index
#define TP_MEMWRITE_INDEX 4 // Memory write with tainted index
#define TP_REP_COUNTER 5    // Instruction with REP prefix and tainted counter
#define TP_SYSENTER 6       // Sysenter

/* Trace format definitions */
#define MAX_NUM_OPERANDS 30 // FNSAVE has a memory operand of 108 bytes
#define MAX_NUM_MEMREGS 5  /* Max number of memregs per memory operand */
#define MAX_NUM_TAINTBYTE_RECORDS 3
#define MAX_STRING_LEN 32
#define MAX_OPERAND_LEN 8 /* Max length of an operand in bytes */
#define MAX_INSN_BYTES 15 /* Maximum number of bytes in a x86 instruction */

#define REGNUM(regid) (regmapping[(regid) - 100])

enum OpUsage { unknown = 0, esp, counter, membase, memindex, memsegment,
  memsegent0, memsegent1 };


typedef struct _taint_byte_record {
  uint32_t source;              // Tainted data source (network,keyboard...)
  uint32_t origin;              // Identifies a network flow
  uint32_t offset;              // Offset in tainted data buffer (network)
} TaintByteRecord;

#define TAINT_RECORD_FIXED_SIZE 1

typedef struct _taint_record {
  uint8_t numRecords;          // How many TaintByteRecord currently used
  TaintByteRecord taintBytes[MAX_NUM_TAINTBYTE_RECORDS];
} taint_record_t;

#define OPERAND_VAL_FIXED_SIZE 32 //Hu
#define OPERAND_VAL_ENUMS_REAL_SIZE 2

typedef struct _operand_val {
  uint8_t access; /* xed_operand_action_enum_t */
  uint8_t length;
  uint64_t tainted_begin;//bitwise taint status at insn begin
  uint64_t tainted_end;//bitwise taint status at insn end
  uint32_t addr;
  uint32_t value;
  enum OpType type;
  enum OpUsage usage;
  taint_record_t records[MAX_OPERAND_LEN];
} OperandVal;

#define ENTRY_HEADER_FIXED_SIZE 24

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
typedef struct _entry_header {
  uint32_t address;
  uint32_t tid;
  uint16_t inst_size;
  uint8_t num_operands;
  uint8_t tp;
  uint32_t eflags;
  uint32_t cc_op;
  uint32_t df;
  char rawbytes[MAX_INSN_BYTES];
  OperandVal operand[MAX_NUM_OPERANDS];
  OperandVal memregs[MAX_NUM_OPERANDS][MAX_NUM_MEMREGS];
} EntryHeader;

typedef struct _proc_record {
  char name[MAX_STRING_LEN];
  uint32_t pid;
  int n_mods;
  uint32_t ldt_base;
} ProcRecord;

typedef struct _module_record {
  char name[MAX_STRING_LEN];
  uint32_t base;
  uint32_t size;
} ModuleRecord;

typedef struct _trace_header {
  int magicnumber;
  int version;
  int n_procs;
  uint32_t gdt_base;
  uint32_t idt_base;
} TraceHeader;

/* Structure to hold trace statistics */
struct trace_stats {
  uint64_t insn_counter_decoded; // Number of instructions decoded
  uint64_t insn_counter_traced; // Number of instructions written to trace
  uint64_t insn_counter_traced_tainted; // Number of tainted instructions written to trace
  uint64_t operand_counter;      // Number of operands decoded
};

/* Exported variables */
extern int received_tainted_data;
extern int has_page_fault;
extern int access_user_mem;
extern int insn_already_written;
extern int regmapping[];
extern long insn_counter_traced; // Instruction counter in trace
extern char filebuf[FILEBUFSIZE];
extern int trace_do_not_write;
extern unsigned int tid_to_trace;
extern int header_already_written;
extern struct trace_stats tstats;

/* Exported Functions */
int get_regnum(OperandVal op);
int getOperandOffset (OperandVal *op);
void decode_address(uint32_t address, EntryHeader *eh);//, int ignore_taint);
unsigned int write_insn(FILE *stream, EntryHeader *eh);
void print_trace_stats(); // Print trace statistics
void clear_trace_stats(); // Clear trace statistics
void xed2_init();   // Initialize disassembler
//int close_trace(FILE *stream);//write trailer to trace and close it 
#endif // _TRACE_H_

