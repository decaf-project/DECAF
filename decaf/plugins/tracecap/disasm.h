/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU GPL, version 3 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/


#ifndef __DISASM_H__
#define __DISASM_H__

#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <bfd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef bfd_get_section_size_before_reloc
#define bfd_get_section_size_before_reloc(x) bfd_get_section_size(x)
#endif



/**************************** Declarations / Defines **************************/

#define STATISTICS

struct i386_instruction;
struct _segment;
struct disassemble_info;


#define INST_STOP   0
#define INST_CONT   1
#define INST_JUMP   2
#define INST_BRANCH 3
#define INST_CALL   4


#define INST_GAP    10



/****************************** Global Variables ******************************/

extern int debug;
extern int statistics;

extern int show_asm;

/****************************** Interface Functions ***************************/
struct i386_instruction* get_i386_insn_buf(const char *buf, size_t size);
struct i386_instruction* get_i386_insn(struct disassemble_info *info, bfd_vma offset, struct _segment *segment);

// AWH - Implemented in disas.c now, instead of shared/disasm.cpp
//extern int buffer_read_memory (bfd_vma memaddr, bfd_byte *myaddr, 
//	unsigned int length, struct disassemble_info *info);

void init_disasm_info(struct disassemble_info *disasm_info, struct _segment *segment);
int is_insn_supported(struct i386_instruction *inst); //added by Heng Yin
void print_i386_insn(struct i386_instruction *inst, int type);
void print_i386_rawbytes(bfd_vma addr, const char *buf, 
				    ssize_t size); 
int get_i386_insn_length (const char *buf);

void disassemble_segment_i386(bfd *abfd, struct disassemble_info *info, bfd_vma from, bfd_vma to);

void disassemble_i386_data(bfd *abfd, int print_insn_digraphs, int interactive);
 
struct i386_instruction* get_inst_of(bfd_vma pc);

struct _segment* get_segment_of(bfd_vma addr);

int buffer_read_memory (bfd_vma memaddr, bfd_byte *myaddr, unsigned int length, struct disassemble_info *info);

int get_control_transfer_type(struct i386_instruction *inst, bfd_vma *target);

void *xalloc(size_t num, size_t size);

void fatal(const char *fmt, ...);

/****************************** Instruction Operands ***************************/

enum OpType { TNone = 0, TRegister, TMemLoc, TImmediate, TJump, TFloatRegister, TMemAddress, TMMXRegister, TXMMRegister, TFloatControlRegister, TDisplacement };

typedef struct _register {
  unsigned char num;
} Register;

typedef struct _float_register {
  unsigned char num;
  unsigned char def;
} FloatRegister;

typedef struct _mem_loc {
  unsigned char base;
  unsigned char index;
  unsigned char scale;
  unsigned char displ_t;
  unsigned long displ;
  unsigned char segment;
} MemLoc;

typedef struct _immediate {
  unsigned char type;
  unsigned long segment;
  unsigned long value;
} Immediate;

typedef struct _jump {
	unsigned char type;
  unsigned char relative;
  long offset;
  bfd_vma target;
} Jump;

typedef struct i386_operand {

  enum OpType type;

  union OpValue {
    Register reg;
    MemLoc mem;
    Immediate imm;
    Jump jmp;
    FloatRegister freg;
  } value;

  unsigned char indirect;

} Operand;


struct disassemble_info {
  bfd_vma current_addr;

  /* These are for buffer_read_memory.  */
  bfd_byte *buffer;
  bfd_vma buffer_vma;
  unsigned int buffer_length;

  /* Number of octets per incremented target address 
     Normally one, but some DSPs have byte sizes of 16 or 32 bits.  */
  unsigned int octets_per_byte;

  /* Some targets need information about the current section to accurately
     display insns.  If this is NULL, the target disassembler function
     will have to make its best guess.  */
  asection *section;
};




/**************************** Queue/Hash Table Structs ************************/

#define HASH_SIZE 128

typedef struct _element {
  bfd_vma addr;
  void *data;
  struct _element *next, *list_next;
  
} Element;

typedef struct _queue {
  struct _element *first, *last;
} Queue;

typedef struct _hash_table {
  Element* table[HASH_SIZE];
  Element* head;
} HashTable;

void push(Queue *, bfd_vma, void *);
void append(Queue *, bfd_vma, void *);
bfd_vma pop(Queue *);
int qexists(Queue *, bfd_vma);
void qclear(Queue *);
void* qdelete(Queue *, bfd_vma);

unsigned int hash(bfd_vma addr);
void insert(HashTable *, bfd_vma, void *);
void* retrieve(HashTable *, bfd_vma);
int hexists(HashTable *, bfd_vma);
void hupdate(HashTable *, bfd_vma);
void hclear(HashTable *);
void* hdelete(HashTable *ht, bfd_vma);


/*************************** Instructions and Blocks **************************/

typedef struct i386_instruction {
  bfd_vma address;
  
  bfd_byte opcode[3]; 
 
  unsigned int prefixes;

  bfd_byte modrm;
  
  Operand ops[3];
  
  unsigned char length;

  struct _segment *segment;

  struct disassemble_info *info;

  /* CFG information */
  
  Queue *children;

  Queue *ancestors;

  int cfg, visited;
#ifdef __GNUG__ /* defined by g++ */
  bool operator<(const struct i386_instruction & other)
  { return this->address < other.address; };
#endif
} Instruction;



typedef struct _basic_block {
  bfd_vma start_addr;
  bfd_vma end_addr;
  int type;
  struct _hash_table instructions;

  Queue cf_up, cf_down;
  Queue conflicts;

  /* for manipulating the basic block graph */
  unsigned char mark, valid, visited, phony;
  unsigned int in_degree;
  struct _basic_block *next;

} BasicBlock;


typedef struct _function {
  bfd_vma start_addr;
  bfd_vma end_addr;
  struct _segment *segment;

  char *name;
  unsigned char *inst_status;
  BasicBlock **inst_block;    
  HashTable blocks;

} Function;  


/****************************** File Segments *********************************/

typedef struct _segment
{
  bfd_byte *data;
  bfd_size_type datasize;
  
  bfd_vma start_addr;
  bfd_vma end_addr;

  asection *section;
  int is_code;

  Instruction **insts;
  unsigned char *status;

  Queue functions;

  struct _segment *next;
} Segment;



/****************************** Obfuscation parameters ************************/

typedef struct _obfuscation_params
{
  bfd_vma table_1, table_2;
  bfd_vma addr_tab;

  unsigned long add_mask, shl_left, shl_right;
} ObfuscationParams;




/****************************** Intel-386 Operations ***************************/

typedef void (*op_rtn)(Instruction *, int, int);

struct i386_op
{
  const char *mnemonic;

  op_rtn op1;
  int bytemode1;
  op_rtn op2;
  int bytemode2;
  op_rtn op3;
  int bytemode3;
};


/* PREFIX flags */
#define PREFIX_REPZ 1
#define PREFIX_REPNZ 2
#define PREFIX_LOCK 4
#define PREFIX_CS 8
#define PREFIX_SS 0x10
#define PREFIX_DS 0x20
#define PREFIX_ES 0x40
#define PREFIX_FS 0x80
#define PREFIX_GS 0x100
#define PREFIX_OP 0x200
#define PREFIX_ADDR 0x400
#define PREFIX_FWAIT 0x800
#define PREFIX_TAKEN 0x1000
#define PREFIX_NOT_TAKEN 0x2000


/*** Address Modes ***/

#define b_mode 1  /* byte operand */
#define w_mode 2  /* word operand */
#define d_mode 3  /* double word operand  */

#define v_mode 5  /* operand size depends on prefixes */


/*** Define Registers ***/

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


#define indir_dx_reg 150


/*** Operand Function Prototypes ***/

void OP_REG(Instruction *, int, int);
void OP_I(Instruction * , int, int);
void OP_sI(Instruction *, int, int);
void OP_E(Instruction *, int, int);
void OP_G(Instruction *, int, int);
void OP_SEG(Instruction *, int, int);
void OP_IMREG(Instruction *, int, int);
void OP_DSreg(Instruction *, int, int);
void OP_ESreg(Instruction *, int, int);
void OP_DIR(Instruction *, int, int);
void OP_OFF(Instruction *, int, int);
void OP_J(Instruction *, int, int);
void OP_indirE(Instruction *, int, int);

/* void OP_C(Instruction *, int, int); */
/* void OP_Rd(Instruction *, int, int); */
/* void OP_D(Instruction *, int, int); */
/* void OP_T(Instruction *, int, int); */

void OP_ST(Instruction *, int, int);
void OP_STi(Instruction *, int, int);


/*** Operand Selectors ***/

#define XX NULL, 0

#define Eb OP_E, b_mode
#define Ew OP_E, w_mode
#define Ed OP_E, d_mode
#define Ev OP_E, v_mode

#define Gb OP_G, b_mode
#define Gd OP_G, d_mode
#define Gw OP_G, w_mode
#define Gv OP_G, v_mode

#define Ib OP_I, b_mode
#define sIb OP_sI, b_mode	/* sign extened byte */
#define Iv OP_I, v_mode
#define Iw OP_I, w_mode

#define eAX OP_REG, eAX_reg
#define eBX OP_REG, eBX_reg
#define eCX OP_REG, eCX_reg
#define eDX OP_REG, eDX_reg
#define eSP OP_REG, eSP_reg
#define eBP OP_REG, eBP_reg
#define eSI OP_REG, eSI_reg
#define eDI OP_REG, eDI_reg

#define AL OP_REG, al_reg
#define BL OP_REG, bl_reg
#define CL OP_REG, cl_reg
#define DL OP_REG, dl_reg

#define AH OP_REG, ah_reg
#define BH OP_REG, bh_reg
#define CH OP_REG, ch_reg
#define DH OP_REG, dh_reg

#define AX OP_REG, ax_reg
#define BX OP_REG, bx_reg
#define CX OP_REG, cx_reg
#define DX OP_REG, dx_reg

#define ES OP_REG, es_reg
#define SS OP_REG, ss_reg
#define CS OP_REG, cs_reg
#define DS OP_REG, ds_reg
#define FS OP_REG, fs_reg
#define GS OP_REG, gs_reg

#define indirDX OP_IMREG, dx_reg
#define Xb OP_DSreg, eSI_reg
#define Xv OP_DSreg, eSI_reg
#define Yb OP_ESreg, eDI_reg
#define Yv OP_ESreg, eDI_reg
#define DSBX OP_DSreg, eBX_reg

#define Ma OP_E, v_mode
#define M OP_E, 0		/* lea, lgdt, etc. */
#define Mp OP_E, 0		/* 32 or 48 bit memory operand for LDS, LES etc */

#define Sw OP_SEG, w_mode

#define Ap OP_DIR, 0

#define Ob64 OP_OFF, b_mode
#define Ov64 OP_OFF, v_mode

#define indirEv OP_indirE, v_mode

#define Jb OP_J, b_mode
#define Jv OP_J, v_mode

// AWH - moved these into disasm-i386.cpp because target-i386/cpu.h has these as a different macro
//#define ST OP_ST, 0
//#define STi OP_STi, 0


/* #define Cm OP_C, d_mode */
/* #define Rd OP_Rd, d_mode */
/* #define Rm OP_Rd, d_mode */
/* #define Dm OP_D, d_mode */
/* #define Td OP_T, d_mode */


extern struct i386_op i386_op_1[];
extern struct i386_op i386_op_2[];

extern const unsigned char onebyte_has_modrm[256];
extern const unsigned char onebyte_is_group[256];
extern const unsigned char twobyte_has_modrm[256];
extern const unsigned char twobyte_is_group[256];
extern const unsigned char twobyte_uses_SSE_prefix[256];

extern const char *intel_names32[];
extern const char *intel_names16[];
extern const char *intel_names8[]; 
extern const char *intel_names_seg[]; 
extern const char *intel_index16[];




/* Floating Point */

#define FGRPd9_2 NULL, NULL, 0, NULL, 0, NULL, 0
#define FGRPd9_4 NULL, NULL, 1, NULL, 0, NULL, 0
#define FGRPd9_5 NULL, NULL, 2, NULL, 0, NULL, 0
#define FGRPd9_6 NULL, NULL, 3, NULL, 0, NULL, 0
#define FGRPd9_7 NULL, NULL, 4, NULL, 0, NULL, 0
#define FGRPda_5 NULL, NULL, 5, NULL, 0, NULL, 0
#define FGRPdb_4 NULL, NULL, 6, NULL, 0, NULL, 0
#define FGRPde_3 NULL, NULL, 7, NULL, 0, NULL, 0
#define FGRPdf_4 NULL, NULL, 8, NULL, 0, NULL, 0

extern struct i386_op float_reg[][8];
extern const char *fgrps[][8];
extern const char *float_mem[];


/* Instruction Groups */

#define GRP0  NULL,  0
#define GRP1  NULL,  1
#define GRP2  NULL,  2
#define GRP3  NULL,  3
#define GRP4  NULL,  4
#define GRP5  NULL,  5
#define GRP6  NULL,  6
#define GRP7  NULL,  7
#define GRP8  NULL,  8
#define GRP9  NULL,  9
#define GRP10 NULL, 10
#define GRP11 NULL, 11
#define GRP12 NULL, 12
#define GRP13 NULL, 13

extern struct i386_op i386_group_1[][8];
extern struct i386_op i386_group_2[][8];

/* Weights */
#define WEIGHT_INST_COUNT 0.25 /**< Instruction count weight */
#define WEIGHT_INST_DIGRAPH 0.5 /**< Instruction digraph weight */
#define WEIGHT_INST_OP_SIZE_MISMATCH 0.9 /**< Instruction operand size mismatch */
#define WEIGHT_INST_MOV_REG_OWN_REF 0.9 /**< Move an instruction into its own reference */
#define WEIGHT_INST_MOV_EBP_ESP_MISMATCH 0.7 /**< Move %esp or %ebp into weird registers */
#define WEIGHT_ARITHMETIC_SPECIAL_REG 0.8 /**< Uncommon arithmetic on a special register */
#define WEIGHT_0x00_OPCODE 0.9 /**< 0x00 opcode */
#define WEIGHT_NOP 0.2 /**< 0x00 opcode */

/**
 * Print error and exit.
 */
/* #define fatal(args...) { \
  fprintf(stderr, args); \
  exit(1); \
  } */

/**
 * Determine if an instruction is a MOV instruction.
 *
 * @param i Instruction.
 * @return True if MOV, otherwise false.
 */
unsigned int is_mov(Instruction *i);

/**
 * Determine if an instruction is a MUL instruction.
 *
 * @param i Instruction.
 * @return True if MUL, otherwise false.
 */
unsigned int is_mul(Instruction *i);

/**
 * Determine if an instruction is a DIV instruction.
 *
 * @param i Instruction.
 * @return True if DIV, otherwise false.
 */
unsigned int is_div(Instruction *i);

/**
 * Determine if an instruction is a SHx instruction.
 *
 * @param i Instruction.
 * @return True if SHx, otherwise false.
 */
unsigned int is_shift(Instruction *i);

/**
 * Determine if an instruction is a less common
 * arithmetic instruction.
 *
 * @param i Instruction.
 * @return True if less common arithmetic instruction,
 *         otherwise false.
 */
unsigned int is_uncommon_arithmetic(Instruction *i);

/**
 * Determine if a register is special-purpose.
 *
 * @param o Operand.
 * @return True if special register, otherwise false.
 */
unsigned int is_special_reg(Operand *o);

/**
 * Instruction digraph node.
 */
typedef struct _inst_digraph_node
{
	struct _inst_digraph_node *next;
	unsigned int count;
	unsigned char opcode[6];
} InstDigraphNode;

/** Digraph hash table size */
#define INST_DIGRAPH_HASH_TABLE_SIZE 1024

/**
 * Instruction digraphs.
 */
typedef struct _inst_digraphs
{
	InstDigraphNode *digraphs[INST_DIGRAPH_HASH_TABLE_SIZE];
  InstDigraphNode *counts[INST_DIGRAPH_HASH_TABLE_SIZE];
	unsigned int count;
} InstDigraphs;

/** Instruction digraphs */
extern InstDigraphs digraphs;

/**
 * Initialize instruction digraphs.
 *
 * @param g Digraphs.
 */
void inst_digraph_init(InstDigraphs *g);

/**
 * Destroy instruction digraphs.
 *
 * @param g Digraphs.
 */
void inst_digraph_destroy(InstDigraphs *g);

/**
 * Instruction digraph hash function.
 *
 * @param o1 Instruction 1 opcodes.
 * @param o2 Instruction 2 opcodes.
 * @return Hash value.
 */
unsigned int inst_digraph_hash(unsigned char *o1, unsigned char *o2);

/**
 * Instruction count hash function.
 *
 * @param o Instruction opcodes.
 * @return Hash value.
 */
unsigned int inst_count_hash(unsigned char *o);

/**
 * Determine if a digraph node is equal to two instructions.
 *
 * @param n Digraph node.
 * @param o1 Instruction 1 opcodes.
 * @param o2 Instruction 2 opcodes.
 * @return True if equal, otherwise false.
 */
unsigned int inst_digraph_node_equals(InstDigraphNode *n,
	unsigned char *o1, unsigned char *o2);

/**
 * Determine if a count node is equal to an instruction.
 *
 * @param n Count node.
 * @param o Instruction opcodes.
 * @return True if equal, otherwise false.
 */
unsigned int inst_count_node_equals(InstDigraphNode *n, unsigned char *o);

/**
 * Increment an instruction digraph count.
 *
 * @param g Digraphs.
 * @param i1 Instruction 1.
 * @param i2 Instruction 2.
 */
void inst_digraph_incr(InstDigraphs *g, Instruction *i1, Instruction *i2);

/**
 * Return probability of an instruction digraph.
 *
 * @param g Digraphs.
 * @param i1 Instruction 1.
 * @param i2 Instruction 2.
 * @return Probability.
 */
float inst_digraph_get(InstDigraphs *g, Instruction *i1, Instruction *i2);

/**
 * Return probability of an instruction.
 *
 * @param g Digraphs.
 * @param i Instruction.
 * @return Probability.
 */
float inst_count_get(InstDigraphs *g, Instruction *i);

/**
 * Put count of instruction digraph.
 *
 * @param g Digraphs.
 * @param o1 Instruction 1 opcodes.
 * @param o2 Instruction 2 opcodes.
 * @param c Count.
 */
void inst_digraph_put(InstDigraphs *g, unsigned char *o1,
	unsigned char *o2, unsigned int c);

/**
 * Put instruction count.
 *
 * @param g Digraphs.
 * @param o Instruction opcodes.
 * @param c Count.
 */
void inst_count_put(InstDigraphs *g, unsigned char *o, unsigned int c);

/**
 * Parse serialized instruction digraphs.
 *
 * @param g Digraphs.
 * @param s Input stream.
 */
void inst_digraph_parse(InstDigraphs *g, FILE *s);

/**
 * Print instruction digraphs.
 *
 * @param g Digraphs.
 */
void inst_digraph_print(InstDigraphs *g);

/**
 * Determine addressing-mode independent register number.
 *
 * @param reg Register number.
 * @return Mode-independent number.
 */
unsigned char indep_regnum(unsigned char reg);

/**
 * Determine width of register.
 *
 * @param reg Register number.
 * @return Register width as addressing mode.
 */
unsigned char reg_width(unsigned char reg);

/**
 * Determine width of operand.
 *
 * @param op Operand.
 * @return Width as addressing mode.
 */
unsigned char op_width(Operand *op);

/**
 * Return a junk score for two instructions.
 *
 * @param last Last instruction.
 * @param cur Current instruction.
 * @return Junk instruction score.
 */
float score_junk_insts(Instruction *last, Instruction *cur);

/**
 * Detect if a given run of instructions is likely
 * junk.
 *
 * @param start Start position.
 * @param end End position.
 * @return Instruction anomaly score.
 *         (0.0 valid, 1.0 junk)
 */
float detect_junk_insts(bfd_vma start, bfd_vma end);

/**
 * Update a previously-computed junk instruction score
 * taking into account a new instruction which prefaces
 * the previous run.
 *
 * @param new New instruction.
 * @param next Next instruction.
 * @param orig Original score.
 * @param num Length of original run.
 * @return Updated junk score.
 */
float update_junk_score(Instruction *newinstr, Instruction *next,
						float orig, unsigned int num);



/******************************************************************************/
/*                           Symbolic Execution Code                          */
/******************************************************************************/


/* struct _machine_state {

  unsigned long registers[8];




} MState;
*/




#ifdef __cplusplus
}
#endif


#endif
