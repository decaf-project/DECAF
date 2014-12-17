#ifndef _TRACE_X86_H_
#define _TRACE_X86_H_

#include <inttypes.h>

/* segment registers */
#define es_reg 100
#define cs_reg 101
#define ss_reg 102
#define ds_reg 103
#define fs_reg 104
#define gs_reg 105

/* address-modifier dependent registers */
#define eAX_reg 108
#define eCX_reg 109
#define eDX_reg 110
#define eBX_reg 111
#define eSP_reg 112
#define eBP_reg 113
#define eSI_reg 114
#define eDI_reg 115

/* 8-bit registers */
#define al_reg 116
#define cl_reg 117
#define dl_reg 118
#define bl_reg 119
#define ah_reg 120
#define ch_reg 121
#define dh_reg 122
#define bh_reg 123

/* 16-bit registers */
#define ax_reg 124
#define cx_reg 125
#define dx_reg 126
#define bx_reg 127
#define sp_reg 128
#define bp_reg 129
#define si_reg 130
#define di_reg 131

/* 32-bit registers */
#define eax_reg 132
#define ecx_reg 133
#define edx_reg 134
#define ebx_reg 135
#define esp_reg 136
#define ebp_reg 137
#define esi_reg 138
#define edi_reg 139
#define LOK
/* Trace header values */
#define VERSION_NUMBER 51 //bit wise tainting
#define MAGIC_NUMBER 0xFFFFFFFF

/* Trace format definitions */
#define MAX_NUM_OPERANDS 30	// FNSAVE has a memory operand of 108 bytes
#define MAX_NUM_MEMREGS 5	/* Max number of memregs per memory operand */
#define MAX_NUM_TAINTBYTE_RECORDS 3
#define MAX_STRING_LEN 32
#define MAX_OPERAND_LEN 8	/* Max length of an operand in bytes */
#define MAX_INSN_BYTES 15	/* Maximum number of bytes in a x86 instruction */

enum OpType { TNone = 0, TRegister, TMemLoc, TImmediate, TJump, TFloatRegister, TMemAddress };
enum OpUsage { unknown = 0, esp, counter, membase, memindex, memsegment, memsegent0, memsegent1 };

enum xed_operand_action_enum_t { 
	XED_OPERAND_ACTION_INVALID, 
	XED_OPERAND_ACTION_RW, 
	XED_OPERAND_ACTION_R, 
	XED_OPERAND_ACTION_W, 
	XED_OPERAND_ACTION_RCW, 
	XED_OPERAND_ACTION_CW, 
	XED_OPERAND_ACTION_CRW, 
	XED_OPERAND_ACTION_CR, 
	XED_OPERAND_ACTION_LAST 
};

#define XED_IS_READ_OPERAND(_t) ((_t == XED_OPERAND_ACTION_RW) || (_t == XED_OPERAND_ACTION_R) || (_t == XED_OPERAND_ACTION_RCW) || (_t == XED_OPERAND_ACTION_CRW) || (_t == XED_OPERAND_ACTION_CR))

#define XED_IS_WRITE_OPERAND(_t) ((_t == XED_OPERAND_ACTION_RW) || (_t == XED_OPERAND_ACTION_W) || (_t == XED_OPERAND_ACTION_RCW) || (_t == XED_OPERAND_ACTION_CRW) || (_t == XED_OPERAND_ACTION_CW))

struct TaintByteRecord 
{
	uint32_t source;	// Tainted data source (network,keyboard...)
	uint32_t origin;	// Identifies a network flow
	uint32_t offset;	// Offset in tainted data buffer (network)
};

#define TAINT_RECORD_FIXED_SIZE 1

struct TaintRecord 
{
	uint8_t numRecords;	// How many TaintByteRecord currently used
	TaintByteRecord taintBytes[MAX_NUM_TAINTBYTE_RECORDS];
};

#define OPERAND_VAL_FIXED_SIZE 32//12

#define OPERAND_VAL_ENUMS_REAL_SIZE 2

struct OperandVal 
{
	uint8_t access; 	/* xed_operand_action_enum_t */
	uint8_t length;
	uint64_t tainted_begin;
	uint64_t tainted_end;
	uint32_t addr;
	uint32_t value;
	enum OpType type;
	enum OpUsage usage;
	TaintRecord records[MAX_OPERAND_LEN];
};

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
	
	memregs[][idx]:  Operands used for indirect addressing
		idx == 0 -> Segment register
		idx == 1 -> Base register
		idx == 2 -> Index register
		idx == 3 -> Segent0
		idx == 4 -> Segent1

	rawybytes[]:   Rawbytes of the x86 instruction
*/
struct EntryHeader 
{
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
};

#define MOD_MAX 1024

struct ProcessRecord 
{
	char name[MAX_STRING_LEN];
	uint32_t pid;
	int n_mods;
	uint32_t ldt_base;
};

struct ModuleRecord
{
	char name[MAX_STRING_LEN];
	uint32_t base;
	uint32_t size;
};

struct TraceHeader 
{
	int magicnumber;
	int version;
	int n_procs;
	uint32_t gdt_base;
	uint32_t idt_base;
};

#define LOK
#endif // _TRACE_X86_H_

