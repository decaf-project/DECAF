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
/******************************************************************************/
/*                       Recursive i386 Disassembler                          */
/******************************************************************************/


#if 0

#include "disasm.h"
#include <stdint.h>
#define HAVE_DECL_BASENAME 1
#include <libiberty.h>
#include <assert.h>

int debug = 0;
int statistics = 0;
/* #define DEFAULT_CALLS
#define DEOBF_PATTERN */

/****************************** Global Variables ******************************/

int show_asm;

jmp_buf failed_read, out_of_segment;

Segment *segs;

int interact;

#ifdef DEOBF_PATTERN
ObfuscationParams *obf_params;
#endif

#ifdef STATISTICS
unsigned long ibb = 0, bb = 0;
unsigned long r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0, r9 = 0; 
#endif


/****************************** Helper Functions ******************************/
#if 0 // AWH - Located in QEMU's disas.c
int buffer_read_memory (bfd_vma memaddr, bfd_byte *myaddr, 
						unsigned int length, struct disassemble_info *info)
{
  unsigned int opb = info->octets_per_byte;
  unsigned int end_addr_offset = length / opb;
  unsigned int max_addr_offset = info->buffer_length / opb; 
  unsigned int octets = (memaddr - info->buffer_vma) * opb;

  if (memaddr < info->buffer_vma
      || memaddr - info->buffer_vma + end_addr_offset > max_addr_offset)
    return -1;
  memcpy (myaddr, info->buffer + octets, length);

  return 0;
}
#endif // AWH

void *xalloc(size_t num, size_t size)
{
  void *chunk = calloc(num, size);
  if (chunk == NULL) {
    printf("memory allocation failed\n");
    exit(1);
  }
  return chunk;
}

void push(Queue *q, bfd_vma addr, void *data)
{
  Element *qe;

  if (q == NULL)
    return;

  qe = (Element *) xalloc(1, sizeof(Element));
  qe->addr = addr;
  qe->data = data;

  if (q->first == NULL)
    q->first = q->last = qe;
  else {
    qe->next = q->first;
    q->first = qe;
  }
}

void append(Queue *q, bfd_vma addr, void *data)
{
  Element *qe;

  if (q == NULL)
    return;

  qe = (Element*) xalloc(1, sizeof(Element));
  qe->addr = addr;
  qe->data = data;

  if (q->first == NULL)
    q->first = q->last = qe;
  else {
    q->last->next = qe;
    q->last = qe;
  }
}

bfd_vma pop(Queue *q)
{
  Element *qe;
  bfd_vma addr;

  if (q == NULL)
    return (bfd_vma) -1;

  if (q->first == NULL)
    return (bfd_vma) -1;

  qe = q->first;

  q->first = q->first->next;
  if (q->first == NULL)
    q->last = NULL;

  addr = qe->addr;
  free(qe);
  return addr;
}

int qexists(Queue *q, bfd_vma addr)
{
  Element *qe;

  if (q == NULL)
    return 0;

  for (qe = q->first; qe != NULL; qe = qe->next)
    if (qe->addr == addr)
      return 1;

  return 0;
}


void* qdelete(Queue *q, bfd_vma addr)
{
  Element *qe, *todel;
  void *data;

  if ((q == NULL) || (q->first == NULL))
    return NULL;

  if (q->first->addr == addr) {
    todel = q->first;
    if (q->first->next == NULL) 
      q->first = q->last = NULL;
    else
      q->first = q->first->next;
    data = todel->data;
    free(todel);
    return data;
  }

  qe = q->first;
  while (qe->next != NULL)
    if (qe->next->addr == addr) {
      todel = qe->next;
      qe->next = todel->next;
      if (qe->next == NULL)
	q->last = qe;
    data = todel->data;
    free(todel);
    return data;
    }
    else 
      qe = qe->next;

  return NULL;
}

void qclear(Queue *q)
{
  Element *qe, *del;

  if (q == NULL)
    return;

  for (qe = q->first; qe != NULL; ) {
    del = qe;
    qe = qe->next;
    free(del);
  }
}

unsigned int hash(bfd_vma addr)
{
  unsigned int result = addr % HASH_SIZE;
  return result;
}


void hupdate(HashTable *ht, bfd_vma addr)
{
  Element *he;
  
  if (ht == NULL)
    return;
  
  he = ht->table[hash(addr)];
  for (; he != NULL; he = he->next) 
    if (he->addr == addr) {
      unsigned long val = (unsigned long) he->data;
      he->data = (void *) (val + 1);
      return;
    }

  he = (Element *) xalloc(1, sizeof(Element));

  he->addr = addr;
  he->data = (void *) 1;

  he->next = ht->table[hash(addr)];
  ht->table[hash(addr)] = he;
  
  he->list_next = ht->head;
  ht->head = he;
}

int hexists(HashTable *ht, bfd_vma addr)
{
  Element *he;
  
  if (ht == NULL)
    return 0;
  
  he = ht->table[hash(addr)];
  for (; he != NULL; he = he->next) 
    if (he->addr == addr)
      return 1;
  return 0;
}

void insert(HashTable *ht, bfd_vma addr, void *data) 
{
  Element *he;

  if ((ht == NULL) || hexists(ht, addr))
    return;

  he = (Element *) xalloc(1, sizeof(Element));

  he->addr = addr;
  he->data = data;

  he->next = ht->table[hash(addr)];
  ht->table[hash(addr)] = he;

  he->list_next = ht->head;
  ht->head = he;
}

void hclear(HashTable *ht)
{
  Element *he, *del;
  int index;

  if (ht == NULL)
    return;

  for (index = 0; index < HASH_SIZE; ++index) {
    he = ht->table[index];
    while (he != NULL) {
      del = he;
      he = he->next;
      free(del);
    }
  }
}
  
void *retrieve(HashTable *ht, bfd_vma addr)
{
  Element *he;
  
  if (ht == NULL)
    return NULL;
  
  he = ht->table[hash(addr)];
  for (; he != NULL; he = he->next) 
    if (he->addr == addr)
      return he->data;
  return NULL;
}

void* hdelete(HashTable *ht, bfd_vma addr)
{
  Element *he, *del;
  void *data;

  if (ht == NULL)
    return NULL;
  
  he = ht->table[hash(addr)];
  if (he == NULL)
    return 0;
  if (he->addr == addr) {
    del = he;
    ht->table[hash(addr)] = he->next;
    goto list_clear;
  }
  for (; he->next != NULL; he = he->next) 
    if (he->next->addr == addr) {
      del = he->next;
      he->next = del->next;
      goto list_clear;
    }
  return NULL;

 list_clear:
  if (del == ht->head) {
    ht->head = del->list_next;
    data = del->data;
    free(del);
    return data;
  }
  else {
    for (he = ht->head; he->list_next != NULL; he = he->list_next)
      if (he->list_next == del) {
	he->list_next = del->list_next;
	data = del->data;
	free(del);
	return data;
      }
  }

  printf("error: inconsistent hash table\n");
  exit(1);
}



/****************************** Disassembler Code *****************************/


/* read a single byte, signal error when necessary */
static bfd_byte
fetch_byte(struct disassemble_info *info)
{
  bfd_byte octet;

  if (buffer_read_memory(info->current_addr++, &octet, 1, info) != 0) {
    longjmp(failed_read, 1);
  }
  return octet;
}

static unsigned char 
get_register(int bytemode, unsigned char regnum)
{
  switch (bytemode) {
  case b_mode:
    return al_reg + regnum;
  case w_mode:
    return ax_reg + regnum;
  case d_mode:
    return eax_reg + regnum;
  default:
    /*    printf("warning: get_register: invalid bytemode\n"); */
    return 0;
  }
}


static unsigned int 
load_signed8(Instruction *inst)
{
  char result = fetch_byte(inst->info);

  inst->length += 1;
  return (unsigned int) result;
}

static unsigned int 
load_8(Instruction *inst)
{
  unsigned char result = fetch_byte(inst->info);

  inst->length += 1;
  return (unsigned int) result;
}

static unsigned int 
load_signed16(Instruction *inst)
{
  int cnt;
  short result = 0;

  for (cnt = 0; cnt < 2; ++cnt) 
    result |= (fetch_byte(inst->info) << cnt*8);

  inst->length += 2;
  return (unsigned int) result;
}

static unsigned int 
load_16(Instruction *inst)
{
  int cnt;
  unsigned short result = 0;

  for (cnt = 0; cnt < 2; ++cnt) 
    result |= (fetch_byte(inst->info) << cnt*8);

  inst->length += 2;
  return (unsigned int) result;
}


static unsigned int 
load_32(Instruction *inst)
{
  int cnt;
  unsigned long result = 0;

  for (cnt = 0; cnt < 4; ++cnt) 
    result |= (fetch_byte(inst->info) << cnt*8);

  inst->length += 4;
  return result;
}



/* This function assumes 32-bit registers as input */
void OP_IMREG(Instruction *inst, int index, int regnum)
{
  Operand op;

  memset(&op, 0, sizeof(Operand));

  op.type = TMemLoc;

  op.value.mem.base = regnum;

  inst->ops[index] = op;
}

void OP_REG(Instruction *inst, int index, int regnum)
{
  Operand op;

  memset(&op, 0, sizeof(Operand));

  op.type = TRegister;

  if ((regnum >= eAX_reg) && (regnum <= eDI_reg)) {
    if (inst->prefixes & PREFIX_OP) 
      regnum += (ax_reg - eAX_reg);       /* 16-bit register */
    else
      regnum += (eax_reg - eAX_reg);      /* 32-bit register */
  }

  op.value.reg.num = regnum;
  
  inst->ops[index] = op;
}

void OP_DSreg(Instruction *inst, int index, int regnum)
{
  Operand op;

  memset(&op, 0, sizeof(Operand));

  op.type = TMemLoc;

  if (inst->prefixes & PREFIX_ADDR) 
    regnum += (ax_reg - eAX_reg);       /* 16-bit register */
  else
    regnum += (eax_reg - eAX_reg);      /* 32-bit register */
  op.value.mem.base = regnum; 

  op.value.mem.segment = ds_reg; 

  inst->ops[index] = op;
}

void OP_ESreg(Instruction *inst, int index, int regnum)
{
  Operand op;

  memset(&op, 0, sizeof(Operand));

  op.type = TMemLoc;

  if (inst->prefixes & PREFIX_ADDR) 
    regnum += (ax_reg - eAX_reg);       /* 16-bit register */
  else
    regnum += (eax_reg - eAX_reg);      /* 32-bit register */
  op.value.mem.base = regnum; 

  op.value.mem.segment = es_reg; 

  inst->ops[index] = op;
}

void OP_E(Instruction *inst, int index, int bytemode)
{
  Operand op;
  unsigned char mod, reg, rm;

  memset(&op, 0, sizeof(Operand));
  mod = (inst->modrm >> 6) & 0x03;
  reg = (inst->modrm >> 3) & 0x07;
  rm =  inst->modrm & 0x07;

  if (bytemode == v_mode) 
    bytemode = ((inst->prefixes & PREFIX_OP) ? w_mode : d_mode);

  if (mod == 3) {
    op.type = TRegister;
    op.value.reg.num = get_register(bytemode, rm);
    goto done;
  }

  if (!(inst->prefixes & PREFIX_ADDR)) {   /* 32-bit address mode */
    op.type = TMemLoc;
    
    
    if (inst->prefixes & PREFIX_CS) 
	op.value.mem.segment = cs_reg;
    else if (inst->prefixes & PREFIX_SS)
	op.value.mem.segment = ss_reg; 
    else if (inst->prefixes & PREFIX_DS)
	op.value.mem.segment = ds_reg; 
    else if (inst->prefixes & PREFIX_ES)
	op.value.mem.segment = es_reg; 
    else if (inst->prefixes & PREFIX_FS)
	op.value.mem.segment = fs_reg; 
    else if (inst->prefixes & PREFIX_GS)
	op.value.mem.segment = gs_reg; 

    /* special case -- directly load 32-bit displacement */ 
    if ((mod == 0) && (rm == 5)) {
      op.value.mem.displ_t = d_mode;
      op.value.mem.displ = load_32(inst);
      goto done;
    }
      
    /* if necessary, load additional SIB byte */
    if (rm == 4) {
      bfd_byte sib = fetch_byte(inst->info);
      unsigned char scale = (sib >> 6) & 0x03; 
      unsigned char index = (sib >> 3) & 0x07;
      unsigned char base  = sib & 0x07;

      inst->length += 1;

      op.value.mem.scale = (1 << scale);

      if (index != 4)
	op.value.mem.index = get_register(d_mode, index);

      if ((mod == 0) && (base == 5)) {
	/* special case -- load only 32-bit displacement and index register */
	op.value.mem.displ_t = d_mode;
	op.value.mem.displ = load_32(inst);
	goto done;
      }
      else {
	/* otherwise, set base register */
	op.value.mem.base = get_register(d_mode, base);
      }
    }
    else {
      if (mod == 3)
	op.value.mem.base = get_register(bytemode, rm);
      else
	op.value.mem.base = get_register(d_mode, rm);
    }

    /* get the additional displacement */
    if (mod == 1) {
      op.value.mem.displ_t = b_mode;
      op.value.mem.displ = load_signed8(inst);
    }
    else if (mod == 2) {
      op.value.mem.displ_t = d_mode;
      op.value.mem.displ = load_32(inst);
    }
  } 
  else {                                  /* 16-bit address mode */


  }

 done:
  inst->ops[index] = op;
}

void OP_G(Instruction *inst, int index, int bytemode)
{
  Operand op;
  unsigned char regnum;

  memset(&op, 0, sizeof(Operand));

  op.type = TRegister;

  regnum = ((inst->modrm >> 3) & 0x07);

  if (bytemode == v_mode) 
    bytemode = ((inst->prefixes & PREFIX_OP) ? w_mode : d_mode);
  op.value.reg.num = get_register(bytemode, regnum);

  inst->ops[index] = op;
}

void OP_SEG(Instruction *inst, int index, __attribute__((unused)) int bytemode)
{
  Operand op;
  unsigned char regnum;

  memset(&op, 0, sizeof(Operand));

  op.type = TRegister;

  regnum = ((inst->modrm >> 3) & 0x07);
  op.value.reg.num = es_reg + regnum;

  inst->ops[index] = op;
}



void OP_I(Instruction *inst, int index, int bytemode)
{
  Operand op;
  memset(&op, 0, sizeof(Operand));

  op.type = TImmediate;

  if (bytemode == v_mode) 
    bytemode = ((inst->prefixes & PREFIX_OP) ? w_mode : d_mode);

  switch (bytemode) {
  case b_mode:
    op.value.imm.type = b_mode;
    op.value.imm.value = load_8(inst);
    break;
  case w_mode:
    op.value.imm.type = w_mode;
    op.value.imm.value = load_16(inst);
    break;
  case d_mode:
    op.value.imm.type = d_mode;
    op.value.imm.value = load_32(inst);
    break;
  }

  inst->ops[index] = op;
}


void OP_sI(Instruction *inst, int index, int bytemode)
{
  Operand op;
  memset(&op, 0, sizeof(Operand));

  op.type = TImmediate;

  if (bytemode == v_mode) 
    bytemode = ((inst->prefixes & PREFIX_OP) ? w_mode : d_mode);

  switch (bytemode) {
  case b_mode:
    op.value.imm.type = b_mode;
    op.value.imm.value = load_signed8(inst);
    break;
  case w_mode:
    op.value.imm.type = w_mode;
    op.value.imm.value = load_signed16(inst);
    break;
  case d_mode:
    op.value.imm.type = d_mode;
    op.value.imm.value = load_32(inst);
    break;
  }

  inst->ops[index] = op;
}
 

void OP_DIR(Instruction *inst, int index, __attribute__((unused)) int bytemode)
{  
  Operand op;
  memset(&op, 0, sizeof(Operand));

  op.type = TImmediate;

  if (inst->prefixes & PREFIX_OP) {
    op.value.imm.value = load_16(inst);
    op.value.imm.segment = load_16(inst);
  }
  else {
    op.value.imm.value = load_32(inst);
    op.value.imm.segment = load_16(inst);
  }
   
  inst->ops[index] = op;
}

void OP_OFF(Instruction *inst, int index, __attribute__((unused)) int bytemode)
{
  Operand op;
  memset(&op, 0, sizeof(Operand));

  op.type = TMemLoc;

  if (inst->prefixes & PREFIX_CS) 
      op.value.mem.segment = cs_reg;
  else if (inst->prefixes & PREFIX_SS)
      op.value.mem.segment = ss_reg; 
  else if (inst->prefixes & PREFIX_DS)
      op.value.mem.segment = ds_reg; 
  else if (inst->prefixes & PREFIX_ES)
      op.value.mem.segment = es_reg; 
  else if (inst->prefixes & PREFIX_FS)
      op.value.mem.segment = fs_reg; 
  else if (inst->prefixes & PREFIX_GS)
      op.value.mem.segment = gs_reg; 

  if (inst->prefixes & PREFIX_ADDR) {
    op.value.mem.displ_t = w_mode;
    op.value.mem.displ = load_16(inst);
  }
  else {
    op.value.mem.displ_t = d_mode;
    op.value.mem.displ = load_32(inst);
  }
   
  inst->ops[index] = op;
}

void OP_indirE(Instruction *inst, int index, int bytemode)
{
  OP_E(inst, index, bytemode);
  inst->ops[index].indirect = 1;
}

void OP_J(Instruction *inst, int index, int bytemode)
{
  Operand op;
  memset(&op, 0, sizeof(Operand));

  op.type = TJump;

  if (bytemode == v_mode) 
    bytemode = ((inst->prefixes & PREFIX_OP) ? w_mode : d_mode);

  op.value.jmp.relative = 1;

  switch (bytemode) {
  case b_mode:
    op.value.jmp.offset = (int) load_signed8(inst);
    break;
  case w_mode:
    op.value.jmp.offset = (int) load_signed16(inst);
    break;
  case d_mode:
    op.value.jmp.offset = (int) load_32(inst);
    break;
  }

  /* compensate for increased PC (add inst->length) */
  op.value.jmp.target = inst->address + inst->length + op.value.jmp.offset;

  /* special case -- if 16-bit mode, mask target with 0xFFFF */
  if (bytemode == w_mode)
    op.value.jmp.target &= 0xFFFF;

  inst->ops[index] = op;
}



/* void OP_C(Instruction *inst, int index, int bytemode) */
/* { */
/* } */

/* void OP_T(Instruction *inst, int index, int bytemode) */
/* { */
/* } */

/* void OP_Rd(Instruction *inst, int index, int bytemode) */
/* { */
/* } */

/* void OP_D(Instruction *inst, int index, int bytemode) */
/* { */
/* } */


void OP_ST (Instruction *inst, int index, __attribute__((unused)) int bytemode)
{
  Operand op;

  memset(&op, 0, sizeof(Operand));

  op.type = TFloatRegister;

  op.value.freg.def = 1;
  
  inst->ops[index] = op;
}

void OP_STi (Instruction *inst, int index, __attribute__((unused)) int bytemode)
{
  Operand op;
  unsigned char regnum;

  /* Register is stored in RM part of ModRM byte */
  regnum = inst->modrm & 0x07;

  memset(&op, 0, sizeof(Operand));

  op.type = TFloatRegister;

  op.value.freg.num = regnum;
  
  inst->ops[index] = op;
}

static void dofloat (Instruction *inst)
{
  struct i386_op *dp;
  unsigned char mod, reg;

  mod = (inst->modrm >> 6) & 0x03;
  reg = (inst->modrm >> 3) & 0x07;
    
  if (mod != 3) {
    if (inst->opcode[0] == 0xdb)
      OP_E (inst, 0, d_mode);
    else if (inst->opcode[0] == 0xdd)
      OP_E (inst, 0, d_mode);
    else
      OP_E (inst, 0, v_mode);
    return;
  }

  dp = &float_reg[inst->opcode[0] - 0xd8][reg];

  if (dp->mnemonic != NULL) {
    if (dp->op1)
      (*dp->op1) (inst, 0, dp->bytemode1);
    if (dp->op2)
      (*dp->op2) (inst, 1, dp->bytemode2);
  }
  else {
    
    /* special case -- instruction fnstsw has %al argument */
    if (inst->opcode[0] == 0xdf && inst->modrm == 0xe0)
      OP_REG(inst, 0, ax_reg);
  }
}




/* disassemble opcode */
static void 
handle_op(Instruction *inst, int num_bytes)
{
  struct i386_op *op_ptr = NULL;

  /* get the optional ModRM byte */
  if ((num_bytes == 1 && onebyte_has_modrm[inst->opcode[0]]) ||
      (num_bytes == 2 && twobyte_has_modrm[inst->opcode[0]]))    
  {
    ++inst->length;
    inst->modrm = fetch_byte(inst->info);
  }

 
  if (num_bytes == 1) {

    if (onebyte_is_group[inst->opcode[0]]) {
      int table_index = i386_op_1[inst->opcode[0]].bytemode1;
      op_ptr = &i386_group_1[table_index][(inst->modrm >> 3) & 0x07];
    }
    else if (inst->opcode[0] >= 0xD8 && inst->opcode[0] <= 0xDF) {
      dofloat(inst);
      return;
    }
    else 
       op_ptr = &i386_op_1[inst->opcode[0]];
 
  } else if (num_bytes == 2) {

    if (twobyte_is_group[inst->opcode[0]]) {
      int table_index = i386_op_2[inst->opcode[0]].bytemode1;
      // In disasm-i386.cpp, i386_group_2 has no entries.
      // op_ptr maybe should be NULL? -- djb 6/13/2007
      assert(0);
      op_ptr = &i386_group_2[table_index][(inst->modrm >> 3) & 0x07];
    }
    else
      op_ptr = &i386_op_2[inst->opcode[0]];
  }

  if (op_ptr->op1)
    (*op_ptr->op1)(inst, 0, op_ptr->bytemode1);

  if (op_ptr->op2)
    (*op_ptr->op2)(inst, 1, op_ptr->bytemode2);

  if (op_ptr->op3)
    (*op_ptr->op3)(inst, 2, op_ptr->bytemode3);
}


static void
print_i386_mnemonic(Instruction *inst);

Instruction* get_i386_insn_buf(const char *buf, size_t size)
{
    struct disassemble_info info; 

    info.octets_per_byte = 1;
    info.buffer = (bfd_byte *)buf;
    info.buffer_vma = 0;
    info.buffer_length = size;
    info.section = 0; 

    Instruction *inst = get_i386_insn(&info, 0, 0); 
    return inst; 
}

int 
get_i386_insn_length (const char *buf)
{
    Instruction *inst = get_i386_insn_buf (buf, 15); 
    return inst->length; 
}

/* disassemble a single instruction */
Instruction* get_i386_insn(struct disassemble_info *info, bfd_vma addr, Segment *segment)
{
  Instruction *inst;
  int cnt;
  bfd_byte octet;


  /* when reading of instruction bytes fails, return NULL */
  info->current_addr = addr;

  inst = (Instruction *) xalloc(1, sizeof(Instruction));

  if (setjmp(failed_read) != 0) {
    free(inst);
    return NULL;
  }

  inst->info = info;
  inst->address = addr;
  inst->segment = segment;

  /* handle (at most four) prefixes */
  for (cnt = 0; cnt < 4; ++cnt) {
    int more_prefixes = 1;

    octet = fetch_byte(info);
    switch (octet) {
    
      /* Group 1: Lock and repeat prefixes */
    case 0xF3: 
      inst->prefixes |= PREFIX_REPZ;
      ++inst->length;
      break;
    case 0xF2: 
      inst->prefixes |= PREFIX_REPNZ;
      ++inst->length;
      break;
    case 0xF0: 
      inst->prefixes |= PREFIX_LOCK;
      ++inst->length;
      break;

      /* Group 2: Segment override and branch hints */
    case 0x2E: 
      inst->prefixes |= PREFIX_CS;
      inst->prefixes |= PREFIX_NOT_TAKEN;
      ++inst->length;
      break;
    case 0x36: 
      inst->prefixes |= PREFIX_SS;
      ++inst->length;
      break;
    case 0x3E: 
      inst->prefixes |= PREFIX_DS;
      inst->prefixes |= PREFIX_TAKEN;
      ++inst->length;
      break;
    case 0x26: 
      inst->prefixes |= PREFIX_ES;
      ++inst->length;
      break;
    case 0x64: 
      inst->prefixes |= PREFIX_FS;
      ++inst->length;
      break;
    case 0x65:
      inst->prefixes |= PREFIX_GS;
      ++inst->length;
      break;

      /* Group 3: Operand-size prefix */
    case 0x66:
      inst->prefixes |= PREFIX_OP;
      ++inst->length;
      break;

      /* Group 4: Address-size prefix */
    case 0x67:
      inst->prefixes |= PREFIX_ADDR;
      ++inst->length;
      break;

    case 0x9b :
      inst->prefixes |= PREFIX_FWAIT;
      ++inst->length;
      break;

    default:
      more_prefixes = 0;
      break;
    }
    
    if (!more_prefixes)
      break;
  }

  /* handle opcode */
  switch (octet) {
  case 0x0F:
    inst->opcode[1] = octet;
    inst->opcode[0] = fetch_byte(info);
    inst->length += 2;
    handle_op(inst, 2);
    break;
  default:
    inst->opcode[0] = octet;
    ++inst->length;
    handle_op(inst, 1);
    break;
  }


  /* require string operations for string prefixes */
  if ((inst->prefixes & PREFIX_REPZ) || (inst->prefixes & PREFIX_REPNZ)) {
    
    switch (inst->opcode[0]) {

    case 0xA4: case 0xA5: case 0xA6: case 0xA7:
    case 0xAA: case 0xAB: case 0xAC: case 0xAD:
    case 0xAE: case 0xAF: case 0x6C: case 0x6D:
    case 0x6E: case 0x6F: case 0x90: case 0xC3:
      return inst;
 /*    case 0xC3:
 	inst->address++;
 	inst->length--; 
 	inst->prefixes &= ~PREFIX_REPZ;
 	return inst;
  */  
    default:
      free(inst);
      return NULL;
    }
  }

  return inst;
}


static void
print_i386_register(int regnum)
{
  if (regnum >= eax_reg)
    printf("%%%s", intel_names32[regnum - eax_reg]);
  else if (regnum >= ax_reg)
    printf("%%%s", intel_names16[regnum - ax_reg]);
  else if (regnum >= al_reg)
    printf("%%%s", intel_names8[regnum - al_reg]);
  else if (regnum >= es_reg)
    printf("%%%s", intel_names_seg[regnum - es_reg]);
  else 
    printf("[unknown register]");
}

static void
print_i386_mnemonic(Instruction *inst)
{
  unsigned int cnt, slength;
  const char *template_val;

  if (inst->opcode[1]) {
    template_val = i386_op_2[inst->opcode[0]].mnemonic;
  }
  else if (inst->opcode[0] >= 0xD8 && inst->opcode[0] <= 0xDF) {

    unsigned char mod, reg, rm;
    
    mod = (inst->modrm >> 6) & 0x03;
    reg = (inst->modrm >> 3) & 0x07;
    rm = inst->modrm & 0x07;

    if (mod != 3)
      template_val = float_mem[(inst->opcode[0] - 0xd8) * 8 + reg];
    else {
      struct i386_op *dp = &float_reg[inst->opcode[0] - 0xd8][reg];
      if (dp->mnemonic)
	template_val = dp->mnemonic;
      else
	template_val = fgrps[dp->bytemode1][rm];
    }
  }
  else {
    if (onebyte_is_group[inst->opcode[0]]) {
      int table_index = i386_op_1[inst->opcode[0]].bytemode1;
      template_val = i386_group_1[table_index][(inst->modrm >> 3) & 0x07].mnemonic;
    }
    else
      template_val = i386_op_1[inst->opcode[0]].mnemonic;
  }


  if (template_val) {
    slength = strlen(template_val);

    for (cnt = 0; cnt < slength; ++cnt, ++template_val) {
      
      if (!isupper(*template_val))
	printf("%c", *template_val);
      else {
	int i;

	switch (*template_val) {
	case 'A':
	  for (i = 0; i < 3; ++i)
	    if (inst->ops[0].type == TRegister)
	      break;
	  if (i == 3)
	    printf("b");
	  break;

	case 'P':
	  if (inst->prefixes & PREFIX_OP)
	    printf("l");
	  break;

	case 'Q':
	  for (i = 0; i < 3; ++i)
	    if (inst->ops[0].type == TRegister)
	      break;
	  if (i == 3) {
	    if (inst->prefixes & PREFIX_OP)
	      printf("w");
	    else
	      printf("l");
	  }
 break;
	  
	case 'E':
	  if (!(inst->prefixes & PREFIX_OP))
	    printf("e");
	  break;

	case 'R':
	case 'F':
	  if (inst->prefixes & PREFIX_OP)
	    printf("w");
	  else
	    printf("l");
	  break;

	case 'W':
	  if (inst->prefixes & PREFIX_OP)
	    printf("b");
	  else
	    printf("w");
	  break;

	case 'N':
	  if (!(inst->prefixes & PREFIX_FWAIT))
	    printf("n");
	  break;

	}
      }
    }
    
    /* branch prediction */
    if (inst->prefixes & PREFIX_NOT_TAKEN)
      printf(",pn");
    if (inst->prefixes & PREFIX_TAKEN)
      printf(",p");

    printf("\t");
  }
}

///This function tells if this instruction is supported by VEX and Vine.
int 
is_insn_supported(Instruction *inst)
{
  const char *template_val;

  if (inst->opcode[1]) {
    template_val = i386_op_2[inst->opcode[0]].mnemonic;
  }
  else if (inst->opcode[0] >= 0xD8 && inst->opcode[0] <= 0xDF) {

    unsigned char mod, reg, rm;
    
    mod = (inst->modrm >> 6) & 0x03;
    reg = (inst->modrm >> 3) & 0x07;
    rm = inst->modrm & 0x07;

    if (mod != 3)
      template_val = float_mem[(inst->opcode[0] - 0xd8) * 8 + reg];
    else {
      struct i386_op *dp = &float_reg[inst->opcode[0] - 0xd8][reg];
      if (dp->mnemonic)
	template_val = dp->mnemonic;
      else
	template_val = fgrps[dp->bytemode1][rm];
    }
  }
  else {
    if (onebyte_is_group[inst->opcode[0]]) {
      int table_index = i386_op_1[inst->opcode[0]].bytemode1;
      template_val = i386_group_1[table_index][(inst->modrm >> 3) & 0x07].mnemonic;
    }
    else
      template_val = i386_op_1[inst->opcode[0]].mnemonic;
  }

  if(template_val != 0 && template_val[0] == '(')
    return 0;

  return 1;
}


void print_i386_insn(Instruction *inst, int type)
{
  int index, needcomma = 0, cnt, remaining, len;
  Segment *segment = inst->segment;

  cnt = inst->length;
  if (cnt > 7) {
    remaining = cnt - 7;
    cnt = 7;
  }
  else
    remaining = 0;

  if (debug) {
    if (type == 0)
      printf("[CFG]");
    else if (type == 1)
      printf("[GAP]");
  }

  /* print address */
#ifdef BFD64
  printf(" %llx:\t", inst->address);
#else
  printf(" %lx:\t", inst->address);
#endif


  /* print up to the first seven bytes */
  if (segment)
      for (len = 0; len < cnt; ++len) 
	  printf("%02x ", segment->data[inst->address - segment->start_addr + len]);

  /* print instruction */
//  if (inst->length >= 6)
//    printf("\t");
//  else if (inst->length >= 3)
//    printf("\t\t");
//  else
//    printf("\t\t\t");
  
  /* print prefixes */
  if (inst->prefixes & PREFIX_REPZ)
    printf("repz ");
  if (inst->prefixes & PREFIX_REPNZ)
    printf("repnz ");


  /* print mnemonic */
  print_i386_mnemonic(inst);

  for (index = 2; index >= 0; --index) {

    switch (inst->ops[index].type) {

    case TRegister:
      if (needcomma)
	printf(",");
      if (inst->ops[index].indirect)
	printf("*");
      print_i386_register(inst->ops[index].value.reg.num);
      needcomma = 1;
      break;

    case TMemAddress: /* TMemAddress was formerly used for LEA, etc. */
    case TMemLoc:
      if (needcomma)
	printf(",");
      if (inst->ops[index].indirect)
	printf("*");
      if (inst->ops[index].value.mem.displ_t)
	printf("0x%lx", inst->ops[index].value.mem.displ);
      if (inst->ops[index].value.mem.segment) {
	print_i386_register(inst->ops[index].value.mem.segment);
	printf(":");
      }
      if (inst->ops[index].value.mem.base || inst->ops[index].value.mem.index)
	printf("(");
      if (inst->ops[index].value.mem.base)
	print_i386_register(inst->ops[index].value.mem.base);
      if (inst->ops[index].value.mem.index) {
	printf(",");
	print_i386_register(inst->ops[index].value.mem.index);
      }
      if (inst->ops[index].value.mem.scale)
	printf(",%d", inst->ops[index].value.mem.scale);
      if (inst->ops[index].value.mem.base || inst->ops[index].value.mem.index)
	printf(")");      
      needcomma = 1;
      break;

    case TImmediate:
      if (needcomma)
	printf(",");
      if (inst->ops[index].value.imm.segment)
	printf("$0x%lx,", inst->ops[index].value.imm.segment);
      printf("$0x%lx", inst->ops[index].value.imm.value);
      needcomma = 1;
      break;

    case TJump:
      if (needcomma)
	printf(",");
#ifdef BFD64
      printf("0x%llx", inst->ops[index].value.jmp.target);
#else
      printf("0x%lx", inst->ops[index].value.jmp.target);
#endif
      needcomma = 1;
      break;

    case TFloatRegister:
      if (needcomma)
	printf(",");
      if (inst->ops[index].value.freg.def)
	printf("%%st");
      else
	printf("%%st(%d)", inst->ops[index].value.freg.num);
      needcomma = 1;
      break;

    // AWH - Four unhandled enum cases, added to shut the compiler up
    case TMMXRegister:
    case TXMMRegister:
    case TFloatControlRegister:
    case TDisplacement:

    case TNone:
      break;
    }
  }
  //printf("\n");

  if (remaining) {
    /* print remaining bytes in batches of 7 */
    for (cnt = 1; remaining > 0; --remaining, ++cnt) {

      if ((cnt % 7) == 1)
#ifdef BFD64
	printf(" %llx:\t", inst->address + cnt + (7 - 1));
#else
	printf(" %lx:\t", inst->address + cnt + (7 - 1));
#endif

      if (segment)
	  printf("%02x ", segment->data[inst->address - segment->start_addr + cnt + (7 - 1)]);
      
//      if ((cnt % 7) == 0)
//	  printf("\n");
    }
//    if ((cnt % 7) != 0)
//      printf("\n");
  }
}

void print_i386_rawbytes(bfd_vma addr, const char *buf, ssize_t size)
{
    Instruction *inst = get_i386_insn_buf(buf, size);
    if (inst) {
	inst->info = 0;
	inst->segment = 0;
	inst->address = addr; 
	print_i386_insn(inst, 0); 
	fflush(stdout);
	free(inst);
    }
}

/* find the segment for memory address addr */
Segment* get_segment_of(bfd_vma addr)
{
  Segment *segment; 

  for (segment = segs; segment != NULL; segment = segment->next)
    if ((addr >= segment->start_addr) && (addr < segment->end_addr))
      return segment;

  return NULL;
}

/* find the instruction of memory address addr */
Instruction* get_inst_of(bfd_vma addr)
{
  unsigned int index;
  Segment *s = get_segment_of(addr);
  if(s == NULL)
	return NULL;

  index = (unsigned int) addr - s->start_addr;

  return s->insts[index];
}


/*
 * Tool specific call/jmp de-obfuscator 
 */


#ifdef DEOBF_PATTERN

#define OBFPAR_ADD_MASK        0x01
#define OBFPAR_SHL_LEFT        0x02
#define OBFPAR_SHL_RIGHT       0x04
#define OBFPAR_TAB_1           0x08
#define OBFPAR_TAB_2           0x10
#define OBFPAR_ADDR_TAB        0x20

#define OBFPAR_COMPLETE 0x3F

static ObfuscationParams* get_obfuscation_params(bfd_vma obf_addr)
{
  Segment *s = get_segment_of(obf_addr);
  bfd_vma pc = obf_addr;
  ObfuscationParams *obfpar;
  unsigned char done = 0;

  if (s == NULL)
    return NULL;

  obfpar = xalloc(1, sizeof(ObfuscationParams));

  while (pc < s->end_addr) {
    
    Instruction *inst = s->insts[pc - s->start_addr];

    if (inst == NULL)
      goto failed;

    pc += inst->length;
    
    switch (inst->opcode[0]) {

    case 0x81:
      if (inst->modrm == 0xE1 && inst->ops[1].type == TImmediate) {
	obfpar->add_mask = inst->ops[1].value.imm.value;
	done |= OBFPAR_ADD_MASK; 
      }
      break;

    case 0xC1:
      if (inst->modrm == 0xE0 && inst->ops[1].type == TImmediate) {
	obfpar->shl_left = inst->ops[1].value.imm.value;
	done |= OBFPAR_SHL_LEFT; 
      }
      else if (inst->modrm == 0xE8 && inst->ops[1].type == TImmediate) {
	obfpar->shl_right = inst->ops[1].value.imm.value;
	done |= OBFPAR_SHL_RIGHT; 
      }
      break;

    case 0xB6:
      if (inst->opcode[1] == 0x0F && inst->modrm == 0x91 && inst->ops[1].type == TMemLoc) {
	obfpar->table_1 = inst->ops[1].value.mem.displ;
	done |= OBFPAR_TAB_1;
      }
      break;

    case 0xB7:
      if (inst->opcode[1] == 0x0F && inst->modrm == 0x8C && inst->ops[1].type == TMemLoc) {
	obfpar->table_2 = inst->ops[1].value.mem.displ;
	done |= OBFPAR_TAB_2;
      }
      break;

    case 0x8B:
      if (inst->modrm == 0x04 && inst->ops[1].type == TMemLoc) {
	obfpar->addr_tab = inst->ops[1].value.mem.displ;
	done |= OBFPAR_ADDR_TAB;
      }
      break;

    case 0xC3:
      if (done == OBFPAR_COMPLETE)
	return obfpar;
      else 
	goto failed;
    }
  }

 failed:
  free(obfpar);
  return NULL;
}

static int pattern_resolve_call(Instruction *call, bfd_vma *call_target, bfd_vma *call_return)
{
  unsigned int index, mask, target, retn;
  bfd_vma addr, readp;
  bfd_byte octet;
  int i;
  Segment *seg;

  *call_target = (bfd_vma) 0;

  *call_return = (bfd_vma) 0;

  if (obf_params == NULL)
    return 0;

  addr = call->address + call->length;

  index = addr & obf_params->add_mask;
 
  mask = (addr << obf_params->shl_left) >> obf_params->shl_right;

  readp = obf_params->table_1 + index;
 
  seg = get_segment_of(readp);
 
  if (seg == NULL)
    return 0;

  octet = seg->data[readp - seg->start_addr];

  index = (unsigned int) octet;

  readp = obf_params->table_2 + 2*index;

  seg = get_segment_of(readp);

  if (seg == NULL)
    return 0;
  
  index = 0;
  for (i = 0; i < 2; ++i) {
    octet = seg->data[readp - seg->start_addr + i];
    index |= (unsigned int) octet << (8 * i);
  }

  readp = obf_params->addr_tab + 8*(index ^ mask);
  
  seg = get_segment_of(readp);

  if (seg == NULL)
    return 0;

  target = 0;
  for (i = 0; i < 4; ++i) {
    octet = seg->data[readp - seg->start_addr + i];
    target |= (unsigned int) octet << (8 * i);
  }

  readp = obf_params->addr_tab + 8*(index ^ mask) + 4;

  seg = get_segment_of(readp);

  if (seg == NULL)
    return 0;

  retn = 0;
  for (i = 0; i < 4; ++i) {
    octet = seg->data[readp - seg->start_addr + i];
    retn |= (unsigned int) octet << (8 * i);
  }

  *call_target = (bfd_vma) target;
  *call_return = (bfd_vma) retn;

  return 1;
}

#else



static unsigned char get_byte(bfd_vma addr)
{
  static Segment *seg_cache = NULL;
  if (seg_cache == NULL || addr < seg_cache->start_addr || addr >= seg_cache->end_addr) {
    seg_cache = get_segment_of(addr);
    if (seg_cache == NULL) 
      longjmp(out_of_segment, 1);
  }
  
  if (seg_cache->data != NULL)
    return seg_cache->data[addr - seg_cache->start_addr];
  else {
    printf("fatal: valid segment has not been loaded\n");
    exit(1);
  }
}

static Instruction* get_instruction(bfd_vma addr)
{
  static Segment *seg_cache = NULL;

  if (seg_cache == NULL || addr < seg_cache->start_addr || addr >= seg_cache->end_addr) {
    seg_cache = get_segment_of(addr);
    if (seg_cache == NULL)
      longjmp(out_of_segment, 1);
  }
  
  if (seg_cache->insts != NULL)
    return seg_cache->insts[addr - seg_cache->start_addr];
  else 
    return NULL;
}


static unsigned long intrpr_calc_addr(unsigned long *registers, Operand *loc, unsigned long offset)
{
  uint64_t target_addr;
  
  if (loc->type != TMemLoc)
    return 0;

  target_addr = loc->value.mem.displ;

  if (loc->value.mem.base)
    target_addr += registers[loc->value.mem.base - eax_reg];

  if (loc->value.mem.index) {
    int scale;
    if (loc->value.mem.scale)
      scale = loc->value.mem.scale;
    else
      scale = 1;
    target_addr += (registers[loc->value.mem.index - eax_reg] * scale);
  }

  target_addr += offset;
   
  return (unsigned long) (target_addr & 0xFFFFFFFF);
}

static unsigned long intrpr_load_byte(unsigned long *registers, Operand *loc, unsigned long offset)
{
  unsigned long addr = intrpr_calc_addr(registers, loc, offset);
  return (unsigned long) get_byte(addr);
}


static void intrpr_resolve_call(Instruction *call, bfd_vma *call_target, bfd_vma *call_return)
{
  unsigned int cnt;
  unsigned long registers[8];
  Instruction *inst;
  bfd_vma target;

  *call_target = (bfd_vma) 0;
  *call_return = (bfd_vma) 0;

  for (cnt = 0; cnt < sizeof(registers)/sizeof(unsigned long); ++cnt)
    registers[cnt] = 0;

  target = call->ops[0].value.jmp.target;

  for (;;) {

    inst = get_instruction(target);

    if (inst == NULL)
      return;

    if (inst->opcode[1] == 0x0F) {
      switch (inst->opcode[0]) {

      case 0xB6: case 0xB7:
	{
	  int regno = inst->ops[0].value.reg.num - eax_reg;
	  
	  registers[regno] = 0;
	  
	  if (inst->opcode[0] == 0xB6)
	    registers[regno] = intrpr_load_byte(registers, &inst->ops[1], 0);
	  else {
	    unsigned long scratch = 0;
	    scratch =  intrpr_load_byte(registers, &inst->ops[1], 0);
	    scratch |=  (intrpr_load_byte(registers, &inst->ops[1], 1) << 8);
	    registers[regno] = scratch;
	  }
	}
	break;
      }
    }
    else {
      
      switch (inst->opcode[0]) {
	
      case 0x81:
	{
	  int dstreg = inst->ops[0].value.reg.num - eax_reg;

	  if (inst->ops[1].type == TRegister) {
	    int srcreg = inst->ops[1].value.reg.num - eax_reg;
	    registers[dstreg] &= registers[srcreg];
	  }
	  else if (inst->ops[1].type == TImmediate) {
	    unsigned long value = inst->ops[1].value.imm.value;
	    registers[dstreg] &= value;
	  }
	}
	break;


      case 0x2d:
	{
	  int dstreg = inst->ops[0].value.reg.num - eax_reg;

	  if (inst->ops[1].type == TRegister) {
	    int srcreg = inst->ops[1].value.reg.num - eax_reg;
	    registers[dstreg] -= registers[srcreg];
	  }
	  else if (inst->ops[1].type == TImmediate) {
	    unsigned long value = inst->ops[1].value.imm.value;
	    registers[dstreg] -= value;
	  }
	}
	break;

      case 0x01:
	{
	  int dstreg = inst->ops[0].value.reg.num - eax_reg;

	  if (inst->ops[1].type == TRegister) {
	    int srcreg = inst->ops[1].value.reg.num - eax_reg;
	    registers[dstreg] += registers[srcreg];
	  }
	  else if (inst->ops[1].type == TImmediate) {
	    unsigned long value = inst->ops[1].value.imm.value;
	    registers[dstreg] += value;
	  }
	}
	break;

      case 0x05:
	{
	  int dstreg = 0;
	  unsigned long value = inst->ops[1].value.imm.value;

	  registers[dstreg] += value;
	}
	break;

      case 0x31:
	{
	  int dstreg = inst->ops[0].value.reg.num - eax_reg;

	  if (inst->ops[1].type == TRegister) {
	    int srcreg = inst->ops[1].value.reg.num - eax_reg;
	    registers[dstreg] ^= registers[srcreg];
	  }
	  else if (inst->ops[1].type == TImmediate) {
	    unsigned long value = inst->ops[1].value.imm.value;
	    registers[dstreg] ^= value;
	  }
	}
	break;


      case 0x8d:
	{
	  int dstreg = inst->ops[0].value.reg.num - eax_reg;
	  registers[dstreg] = intrpr_calc_addr(registers, &inst->ops[1], 0);
	}
	break;
	
      case 0x89: case 0x8b:
	
	if (inst->ops[0].type == TRegister && inst->ops[1].type == TRegister) {
	  
	  int srcreg = inst->ops[1].value.reg.num - eax_reg;
	  int dstreg = inst->ops[0].value.reg.num - eax_reg;
	  registers[dstreg] = registers[srcreg];
	}
	else if (inst->ops[0].type == TMemLoc) {

	  int regno = inst->ops[1].value.reg.num - eax_reg;

	  if (inst->ops[0].value.mem.base == esp_reg) {
	    if (inst->ops[0].value.mem.displ == 0x10)
	      *call_target = (bfd_vma) registers[regno];
	    else if (inst->ops[0].value.mem.displ == 0x14)
	      *call_return = (bfd_vma) registers[regno];
	  }

	} else if (inst->ops[1].type == TMemLoc) {

	  int regno = inst->ops[0].value.reg.num - eax_reg;

	  if (inst->ops[1].value.mem.base == esp_reg)
	    registers[regno] = call->address + call->length;
	  else {
	    unsigned long scratch = 0;

	    scratch =  intrpr_load_byte(registers, &inst->ops[1], 0);
	    scratch |= intrpr_load_byte(registers, &inst->ops[1], 1) << 8;
	    scratch |= intrpr_load_byte(registers, &inst->ops[1], 2) << 16;
	    scratch |= intrpr_load_byte(registers, &inst->ops[1], 3) << 24;

	    registers[regno] = scratch;
	  }
	}
	break;
	
      case 0xC1:
	{
	  int grpind = (inst->modrm >> 3) & 0x07;
	  int regno = inst->ops[0].value.reg.num - eax_reg;
	  unsigned long sval = inst->ops[1].value.imm.value;
	  
	  if (grpind == 4)
	    registers[regno] = registers[regno] << sval;
	  else
	    registers[regno] = registers[regno] >> sval;
	}
	break;

      case 0xC3:
	return;
      }
    }

    target += inst->length;
  }
}

#endif


static int resolve_call(Instruction *call, bfd_vma *call_target, bfd_vma *call_return)
{
  bfd_vma target, retn;

  *call_target = (bfd_vma) 0;
  *call_return = (bfd_vma) 0;

  if (setjmp(out_of_segment) != 0) 
    return 0;

#ifdef DEOBF_PATTERN
  pattern_resolve_call(call, &target, &retn);
#else
  intrpr_resolve_call(call, &target, &retn);
#endif

  if (target != 0) {
    *call_target = (bfd_vma) target;
    *call_return = (bfd_vma) retn;
    /* printf("call resolved: %lx --> %lx (returns %lx)\n", call->address, target, retn); */
    return 1;
  }
  else
    return 0;
}




int get_control_transfer_type(Instruction *inst, bfd_vma *target)
{


  
  if (inst->opcode[1] != 0) {
    
    switch (inst->opcode[0]) {
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: 
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: 
      *target = inst->ops[0].value.jmp.target;
      return INST_BRANCH;
    }
  }
  else {

    switch (inst->opcode[0]) {

      /* jump if condition is met (i.e. branch), jEcxz, loops */
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: 
    case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: 
    case 0xE3:
    case 0xE0: case 0xE1: case 0xE2:
      *target = inst->ops[0].value.jmp.target;
      return INST_BRANCH;

      /* near call */
    case 0xE8:
      *target = inst->ops[0].value.jmp.target;
      return INST_CALL;
      
      /* jmp and short jmp */
    case 0xEB:
    case 0xE9:
      *target = inst->ops[0].value.jmp.target;
      return INST_JUMP;

      /* returns */
    case 0xC2: case 0xC3:
    case 0xCA: case 0xCB:
      return INST_STOP;
      
      /* far, absolute call */
    case 0x9A:
      *target = (bfd_vma) -1;
      return INST_CALL;
      
      /* far, absolute jmp */
    case 0xEA:
      *target = (bfd_vma) -1;
      return INST_JUMP;
      
    case 0xFF:
      {
	unsigned char grp_ptr = ((inst->modrm >> 3) & 0x07);
	switch (grp_ptr) {
	case 0: case 1: case 6: case 7:
	  return INST_CONT;
	  
	case 2: case 3:
	  /* near and far absolute indirect call */
	  *target = (bfd_vma) -1;
	  return INST_CALL;
	  
	  /* near and far absolute indirect jump */
	case 4: case 5:
	  *target = (bfd_vma) -1;
	  return INST_JUMP;
	}
      }
      break;
      
      /* hlt stops */
    case 0xF4:
      return INST_STOP;
    }
  }

  return INST_CONT;
}


static void set_control_flow(BasicBlock *from, BasicBlock *to)
{
  if (!from || !to) {
    printf("fatal: set_control_flow: illegal control flow\n");
    exit(1);
  }

  if (!qexists(&from->cf_down, to->start_addr))
    append(&from->cf_down, to->start_addr, to);
  
  if (!qexists(&to->cf_up, from->start_addr))
    append(&to->cf_up, from->start_addr, from);
}

static void remove_block(HashTable *blocks, BasicBlock *block)
{
  Element *e;
  BasicBlock *b;

  /* printf("removing %lx\n", block->start_addr); */

  /* roll back control flow and conflict entries */
  for (e = block->cf_up.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;
    qdelete(&b->cf_down, block->start_addr);
  }
  for (e = block->cf_down.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;
    qdelete(&b->cf_up, block->start_addr);
  }
  for (e = block->conflicts.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;
    qdelete(&b->conflicts, block->start_addr);
  }

  /* remove basic block from blocks hash table */
  hdelete(blocks, block->start_addr);

  /* free the basic block and its data structures */
  hclear(&block->instructions);
  qclear(&block->cf_up);
  qclear(&block->cf_down);
  qclear(&block->conflicts);
  free(block);
}

static void remove_referer_entry(BasicBlock *block, BasicBlock **refer_block, unsigned int refer_size)
{
  unsigned int i;

  for (i = 0; i < refer_size; ++i) 
    if (refer_block[i] == block)
      refer_block[i] = NULL;
}


static void clean_block_list(HashTable *blocks)
{
  Element *e;
  BasicBlock *b;
  int index;

  for (index = 0; index < HASH_SIZE; ++index) 
    for (e = blocks->table[index]; e != NULL; e = e->next) {
      b = (BasicBlock *) e->data;
    
      b->visited = 0;
    }
}


static void remove_marked_blocks(BasicBlock **blist, HashTable *blocks)
{
  BasicBlock *block, *del;

  while (*blist && (*blist)->mark) {
    block = *blist;

    *blist = (*blist)->next;
    remove_block(blocks, block);
  }

  for (block = *blist; block && block->next != NULL; ) {

    if (block->next->mark) {
      del = block->next;
      block->next = del->next;
      remove_block(blocks, del);
    }
    else
      block = block->next;
  }
}  

static int get_ancestor_cnt_rec(BasicBlock *block)
{
  BasicBlock *b;
  Element *e;
  int retval = 1;

  if (block->visited)
    return 0;
  else
    block->visited = 1;

  for (e = block->cf_up.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;
    retval += get_ancestor_cnt_rec(b);
  }

  return retval;
}

static int get_ancestor_cnt(HashTable *blocks, BasicBlock *block)
{
  int retval = get_ancestor_cnt_rec(block);
  clean_block_list(blocks);
  return retval;
}


static void remove_successor_blocks_rec(HashTable *blocks, BasicBlock *block, BasicBlock **ref, unsigned int ref_size)
{
  Element *child, *next;

  if (block->visited)
    return;
  else
    block->visited = 1;

  /* if current node has only one parent that will be removed, continue removing  */
  if (block->cf_up.first != NULL && block->cf_up.first->next == NULL) {

    child = block->cf_down.first; 

    while (child != NULL) {
      next = child->next;
      remove_successor_blocks_rec(blocks, block, ref, ref_size);
      child = next;
    }

    remove_referer_entry(block, ref, ref_size);
    remove_block(blocks, block);
  }
}

static void remove_successor_blocks(HashTable *blocks, BasicBlock *block, BasicBlock **ref, unsigned int ref_size)
{
  Element *child, *next;

  /* don't remove this block */
  block->visited = 1;

  child = block->cf_down.first; 

  while (child != NULL) {
    next = child->next;
    remove_successor_blocks_rec(blocks, block, ref, ref_size);
    child = next;
  }

  clean_block_list(blocks);
  
}

static void set_reachable(BasicBlock *block)
{
  Element *child;


  if (block->valid)
    return;
  else
    block->valid = 1;

  for (child = block->cf_down.first; child != NULL; child = child->next) {
    set_reachable((BasicBlock *) child->data);
  }
}

static void merge_blocks(HashTable *blocks)
{
  Element *e1, *e2;
  BasicBlock *b1, *b2, *swap;

  /* no blocks to merge */
  if (blocks->head == NULL)
    return;

  for (e1 = blocks->head; e1->list_next != NULL; e1 = e1->list_next) {
    for (e2 = e1->list_next; e2 != NULL; e2 = e2->list_next) {
      
      b1 = (BasicBlock *) e1->data;
      b2 = (BasicBlock *) e2->data;

      if (b1->start_addr > b2->start_addr) {
	swap = b1; b1 = b2; b2 = swap;
      }
      
      /*
       *   the start address of basic blocks is unique (hash key) 
       *   now: start address of first block < start address of second block 
       */
      if (b1->end_addr <= b2->start_addr)
	; /* b1 completely before b2 */
      else if (b1->end_addr < b2->end_addr) {
	append(&b1->conflicts, b2->start_addr, b2);
	append(&b2->conflicts, b1->start_addr, b1);
      }
      else if (b1->end_addr == b2->end_addr) {

	if ((hexists(&b1->instructions, b2->start_addr)) &&
	    b1->instructions.head &&
	    b1->instructions.head->list_next == NULL)
	  {
	  /*
	   * second block is a sub-block of first one:
	   * 
	   *   conflicts and successor nodes are identical, so merge only
	   *   upstream links and mark second block for deletion
	   */


	  /* 
	   * that should no longer happen because of improved algorithm :)
	   */
	  printf("fatal: merging %lx into %lx\n", (unsigned long) b2->start_addr, (unsigned long) b1->start_addr);
	  exit(1);

#ifdef accurate_tracking
	  Element *up_e;
	  BasicBlock *up_b;
	  for (up_e = b2->cf_up.first; up_e != NULL; up_e = up_e->next) {
	    up_b = (BasicBlock *) up_e->data;
	    set_control_flow(up_b, b1);
	  }
#endif
	  b2->mark = 1;
	}
	else {
	  append(&b1->conflicts, b2->start_addr, b2);
	  append(&b2->conflicts, b1->start_addr, b1);
	}
      }
      else {
	append(&b1->conflicts, b2->start_addr, b2);
	append(&b2->conflicts, b1->start_addr, b1);
      }
    }
  }

  for (e1 = blocks->head; e1->list_next != NULL; ) {
    e2 = e1->list_next;

    b1 = (BasicBlock *) e1->data;
    if (b1->mark) 
      remove_block(blocks, b1);

    e1 = e2;
  }
}
   
static void sort_topological(BasicBlock *block, BasicBlock **blist)
{
  Element *child;
  BasicBlock *b;

  if (block->visited)
    return;
  else
    block->visited = 1;

  for (child = block->cf_down.first; child != NULL; child = child->next) {
    b = (BasicBlock *) child->data;
    sort_topological(b, blist);
  }

  block->next = *blist;
  *blist = block;
} 

static void mark_path_rec(BasicBlock *block)
{
  Element *child;
  BasicBlock *b;

  if (block->visited)
    return;
  block->visited = 1;

#ifdef STATISTICS
  if (!block->mark) r1++;
#endif
  block->mark = 1;

  for (child = block->cf_up.first; child != NULL; child = child->next) {
    b = (BasicBlock *) child->data;
    mark_path_rec(b);
  }
}

static void mark_path(BasicBlock *block)
{
  mark_path_rec(block);
}

static int is_reachable(BasicBlock *target, BasicBlock *from)
{
  Element *e;
  BasicBlock *b;

  if (from->visited)
    return 0;
  else
    from->visited = 1;

  if (from->start_addr == target->start_addr)
    return 1;
  
  for (e = from->cf_down.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;

    if (is_reachable(target, b))
      return 1;
  }
  return 0;
}

static int has_good_nodes(BasicBlock *block)
{
  Element *e;
  BasicBlock *b;

  if (block->visited)
    return 0;
  else
    block->visited = 1;

  if (block->conflicts.first == NULL)
    return 1;

  for (e = block->cf_down.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;
    if (has_good_nodes(b))
      return 1;
  }
  return 0;
}

static void mark_ancestors(BasicBlock *block, int run)
{
  BasicBlock *b;
  Element *e;

  if (block->visited == run)
    return;
  
  if (block->visited > 0) {
#ifdef STATISTICS
    if (!block->mark) r3++;
#endif
    block->mark = 1;
  }

  block->visited = run;

  for (e = block->cf_up.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;
    mark_ancestors(b, run);
  }
}

static void mark_phony_branches_rec(BasicBlock *block)
{
  BasicBlock *b;
  Element *e;

  if (block->visited)
    return;
  else {
    block->visited = 1;
    block->phony = 1;
  }

  for (e = block->cf_up.first; e != NULL; e = e->next) {
    b = (BasicBlock *) e->data;
    mark_phony_branches_rec(b);
  }
}

static void mark_phony_branches(HashTable *blocks, BasicBlock *block)
{
  mark_phony_branches_rec(block);
  clean_block_list(blocks);
}


static void remove_common_ancestors(HashTable *blocks, BasicBlock *b1, BasicBlock *b2)
{
  mark_ancestors(b1, 1);
  mark_ancestors(b2, 2);
  clean_block_list(blocks);
}


static void resolve_conflicts(HashTable *blocks)
{
  Element *e, *e2;
  BasicBlock *blist, *b, *b2;

  /* 
   *   0. Sort all blocks in topological order of control flow graph
   */
  blist = NULL;

  for (e = blocks->head; e != NULL; e = e->list_next) {
    b = (BasicBlock *) e->data;
    b->in_degree = 0;
    for (e2 = b->cf_up.first; e2 != NULL; e2 = e2->next)
      b->in_degree++;
  }

  for (e = blocks->head; e != NULL; e = e->list_next) {
    b = (BasicBlock *) e->data;
    if (!b->in_degree) {
      sort_topological(b, &blist);
    }
  }
  /* in a final sweep -- take care of cycles */
  for (e = blocks->head; e != NULL; e = e->list_next) {
    b = (BasicBlock *) e->data;
    if (!b->visited) {
      sort_topological(b, &blist);
    }
  }
  clean_block_list(blocks);


  /* 
   *   1. Remove all blocks that conflict with known valid blocks
   */
  for (b = blist; b != NULL; b = b->next)     
    if (!b->mark && !b->valid) 
      for (e = b->conflicts.first; e != NULL; e = e->next) {
	b2 = (BasicBlock *) e->data;

	if (b2->valid) {
	  mark_path(b);
	  clean_block_list(blocks);
	}
      }
  if (debug)
    printf("Test 1: Remove blocks that conflict with known valid blocks\n");
  remove_marked_blocks(&blist, blocks);



  /* 
   * 1.8 Special branch handling
   */
  for (b = blist; b != NULL; b = b->next) {
    if (!b->mark) {
      for (e = b->conflicts.first; e != NULL; e = e->next) {
	b2 = (BasicBlock *) e->data;
	if (!b2->mark)
	  
	  if (b->cf_up.first != NULL &&
	      b->cf_up.first->next == NULL &&
	      b2->cf_up.first != NULL &&
	      b2->cf_up.first->next == NULL) {

	    BasicBlock *p1, *p2;
	    
	    p1 = (BasicBlock *) b->cf_up.first->data;
	    p2 = (BasicBlock *) b2->cf_up.first->data;

	    if (p1 == p2) {
#ifdef STATISTICS
	      if (!b->mark) r2++;
	      if (!b2->mark) r2++;
	      if (!p1->mark) r2++;
#endif
	      b->mark = b2->mark = p1->mark = 1; 
	      goto loop15;
	    }
	  }
      }
    }
  loop15:
    ;
  }
  if (debug)
    printf("Test 1.5 (experimental): Remove common ancestors of conflicting nodes\n");
  remove_marked_blocks(&blist, blocks);



  /*
   * 2. Improved version: Remove all common ancestors of conflicting nodes
   */
  for (b = blist; b != NULL; b = b->next) {
    if (!b->mark) {
      for (e = b->conflicts.first; e != NULL; e = e->next) {
	b2 = (BasicBlock *) e->data;
	if (!b2->mark)
	  remove_common_ancestors(blocks, b, b2);
      }
    }
  }
  if (debug)
    printf("Test 2: Remove common ancestors of conflicting nodes\n");
  remove_marked_blocks(&blist, blocks);



  /*
   * 2.5. Remove a conflicting node that invalidates more predecessor nodes
   */
  for (b = blist; b != NULL; b = b->next) {
    if (!b->mark) {
      for (e = b->conflicts.first; e != NULL; e = e->next) {
	b2 = (BasicBlock *) e->data;
	if (!b2->mark) {
	  int ancestor_1, ancestor_2;

	  ancestor_1 = get_ancestor_cnt(blocks, b);
	  ancestor_2 = get_ancestor_cnt(blocks, b2);

	  if (ancestor_1 > ancestor_2) {
#ifdef STATISTICS
	    if (!b2->mark) r4++;
#endif
	    b2->mark = 1; 
	    mark_phony_branches(blocks, b2);
	  }
	  else if (ancestor_2 > ancestor_1) {
#ifdef STATISTICS
	    if (!b->mark) r4++;
#endif
	    b->mark = 1;
	    mark_phony_branches(blocks, b);
	    goto loop25;
	  }
	}
      }
    }
  loop25:
    ;
  }
  if (debug)
    printf("Test 3: Remove node with less predecessors\n");
  remove_marked_blocks(&blist, blocks);


  /* 
   * 2.7. Remove phony branches
   */
  for (b = blist; b != NULL; b = b->next) {
    if (!b->mark &&
	b->type == INST_BRANCH &&
	b->instructions.head != NULL &&
	b->instructions.head->list_next == NULL &&
	b->phony > 0) {
#ifdef STATISTICS
      if (!b->mark) r5++;
#endif
      b->mark = 1;
    }
  }
  if (debug)
    printf("Test 4: Remove phony branches\n");
  remove_marked_blocks(&blist, blocks);


  /*
   *   3. Remove conflicting blocks without "good" (i.e. non-conflicting) nodes 
   */
  for (b = blist; b != NULL; b = b->next) {
    if (!b->mark)
      for (e = b->conflicts.first; e != NULL; e = e->next) {
	b2 = (BasicBlock *) e->data;
	if (!b2->mark) {
	  int good1, good2;
	  good1 = has_good_nodes(b);
	  clean_block_list(blocks);
	  good2 = has_good_nodes(b2);
	  clean_block_list(blocks);

	  if (good1 && !good2) {
#ifdef STATISTICS
	    if (!b2->mark) r6++;
#endif
	    b2->mark = 1; 
	  }
	  else if (!good1 && good2) {
#ifdef STATISTICS
	    if (!b->mark) r6++;
#endif
	    b->mark = 1; 
	    goto loop3;
	  }
	}
      }
  loop3:
    ;
  }
  if (debug)
    printf("Test 5: Remove nodes without 'good' children\n");
  remove_marked_blocks(&blist, blocks);


  /*
   *   4. Conflicts must be resolved here -> remove block with more conflicts
   */
  for (b = blist; b != NULL; b = b->next) {
    if (!b->mark) {
      int conf_cnt_1 = 0, child_cnt_1 = 0;
      for (e = b->conflicts.first; e != NULL; e = e->next) 
	conf_cnt_1++;
      for (e = b->cf_down.first; e != NULL; e = e->next) 
	child_cnt_1++;

      for (e = b->conflicts.first; e != NULL; e = e->next) {
	b2 = (BasicBlock *) e->data;
	if (!b2->mark) {

	  /* take the node with lesser conflicts */
	  int conf_cnt_2 = 0, child_cnt_2 = 0;
	  for (e2 = b2->conflicts.first; e2 != NULL; e2 = e2->next) 
	    conf_cnt_2++;
	  for (e2 = b2->cf_down.first; e2 != NULL; e2 = e2->next) 
	    child_cnt_2++;

	  if (conf_cnt_1 > conf_cnt_2) {
#ifdef STATISTICS
	    if (!b->mark) r7++;
#endif
	    b->mark = 1; 
	    goto loop4;
	  } 
	  else if (conf_cnt_2 > conf_cnt_1) {
#ifdef STATISTICS
	    if (!b2->mark) r7++;
#endif
	    b2->mark = 1;
	    continue;
	  }

	  /* take the node with lesser child nodes */
	  if (child_cnt_1 < child_cnt_2) {
#ifdef STATISTICS
	    if (!b->mark) r8++;
#endif
	    b->mark = 1; 
	    goto loop4;
	  } 
	  else if (child_cnt_1 > child_cnt_2) {
#ifdef STATISTICS
	    if (!b2->mark) r8++;
#endif
	    b2->mark = 1; 
	    continue;
	  }

	  /* randomly take the second node */
#ifdef STATISTICS
	  if (!b2->mark) r9++;
#endif
	  b2->mark = 1; 

	}
      }
    }
 loop4:
    ;
  }  
  if (debug)
    printf("Test 6: Last resort heuristics\n");
  remove_marked_blocks(&blist, blocks);
}



static BasicBlock *extract_sub_block(BasicBlock *parent, HashTable *blocks, bfd_vma addr, bfd_vma func_start, BasicBlock **block_refer)
{
  BasicBlock *block, *next;
  bfd_vma pc, target;
  Element *e;

  block = (BasicBlock *) xalloc(1, sizeof(BasicBlock));
  block->start_addr = addr;
  block->end_addr = parent->end_addr;

  /* copy instructions */
  for (pc = addr; pc < parent->end_addr; pc += get_inst_of(pc)->length) {
    if (!hexists(&parent->instructions, pc)) {
      printf("fatal: extract_sub_block: inconsistent block structure (instructions)\n");
      exit(1);
    }
    hdelete(&parent->instructions, pc);
    insert(&block->instructions, pc, get_inst_of(pc));
      
    if (block_refer[pc - func_start] != parent) {
      printf("fatal: extract_sub_block: inconsistent block structure (block referrer)\n");
      exit(1);
    }
    block_refer[pc - func_start] = block;
  }

  /* set new end address */
  parent->end_addr = addr;

  /* set types */
  block->type = parent->type;
  parent->type = INST_CONT;
    
  /* correct upstream and downstream links */
  while ((target = pop(&parent->cf_down)) != (bfd_vma) -1) 
    append(&block->cf_down, target, retrieve(blocks, target));   

  append(&parent->cf_down, addr, block);
  append(&block->cf_up, parent->start_addr, parent);

  for (e = block->cf_down.first; e != NULL; e = e->next) { 
    next = (BasicBlock *) retrieve(blocks, e->addr);

    if ((next == NULL) || !qexists(&next->cf_up, parent->start_addr)) {
      printf("fatal: extract_sub_block: inconsistent block structure (parent/child)\n");
      exit(1);
    }
    
    qdelete(&next->cf_up, parent->start_addr);
    append(&next->cf_up, block->start_addr, block);
  }
  
  return block;
}




#define UNUSED        0x0
#define INST          0x1
#define NO_INST       0x2

#define BLOCK_START   0x8

#define MAX_INST_LEN  15

#define SEARCH_MODE    0
#define LINEAR_MODE    1


static BasicBlock* get_basic_blocks(bfd_vma addr, 
				    HashTable *blocks, 
				    unsigned char *grid, 
				    bfd_vma func_start,
				    bfd_vma func_end, 
				    BasicBlock **block_refer)
{
  BasicBlock *block, *next, *ref;
  Instruction *inst;
  bfd_vma target, pc;
  unsigned int refer_size = (func_end - func_start);

  /* get the basic block that this instruction is part of */
  ref = block_refer[addr - func_start];

  /* if instruction is first of basic block, just return the basic block */
  if (ref && ref->start_addr == addr)
    return ref;
  /* 
   *   if instruction refers to basic block but is not its first, then
   *   we jumped into an existing block. 
   *   This block needs to be split into two subblocks.
   */
  else if (ref) {
    block = extract_sub_block(ref, blocks, addr, func_start, block_refer);
    insert(blocks, block->start_addr, block);

    return block;
  }

  /* else make a new block */
  block = (BasicBlock *) xalloc(1, sizeof(BasicBlock));
  block->start_addr = addr;
  insert(blocks, block->start_addr, block);

  
  /* 
   * start the linear disassemble until either:
   * 1. a control flow operation is encountered or 
   * 2. an instruction is encountered that belongs to another basic block
   *
   */
  pc = addr;

  for (;;) {

    /* check for second case first */
    if ((ref = block_refer[pc - func_start]) != NULL) {
      
      if (ref->start_addr == pc) 
	next = ref;
      else { 
	next = extract_sub_block(ref, blocks, pc, func_start, block_refer);
	insert(blocks, next->start_addr, next);
      }

      append(&block->cf_down, next->start_addr, next);
      append(&next->cf_up, block->start_addr, block);

      block->end_addr = pc;
      block->type = INST_CONT;
      return block;
    }

    /* else, analyze instruction (special case -- continuation to end of function) */
    if (pc == func_end) {
      block->end_addr = pc;
      block->type = INST_STOP;
      return block;
    }

    inst = get_inst_of(pc);
    if (inst == NULL || (grid[pc - func_start] & NO_INST)) {
      goto failed;
    }

    insert(&block->instructions, inst->address, inst);
    block_refer[pc - func_start] = block;

    switch (get_control_transfer_type(inst, &target)) {
    case INST_BRANCH:
      block->end_addr = inst->address + inst->length;
      block->type = INST_BRANCH;
      
      if ((target >= func_start && target < func_end)) {
	next = get_basic_blocks(target, blocks, grid, func_start, func_end, block_refer);
	if (next == NULL) {
	  remove_successor_blocks(blocks, block, block_refer, refer_size);
	  goto failed;
	}
	else
	  set_control_flow(block, next);
      }
      if ((block->end_addr >= func_start && block->end_addr < func_end)) {
	next = get_basic_blocks(block->end_addr, blocks, grid, func_start, func_end, block_refer);
	if (next == NULL) {
	  remove_successor_blocks(blocks, block, block_refer, refer_size);
	  goto failed;
	}
	else
	  set_control_flow(block, next);
      }
      return block;
      
    case INST_CONT:
      pc += inst->length;
      break;

    case INST_CALL:
      block->end_addr = inst->address + inst->length;
      block->type = INST_CALL;

      if ((target >= func_start && target < func_end)) {
	next = get_basic_blocks(target, blocks, grid, func_start, func_end, block_refer);
	if (next == NULL) {
	  remove_successor_blocks(blocks, block, block_refer, refer_size);
	  goto failed;
	}
	else
	  set_control_flow(block, next);
      }
#ifdef DEFAULT_CALLS
      if ((block->end_addr >= func_start && block->end_addr < func_end)) {
	next = get_basic_blocks(block->end_addr, blocks, grid, func_start, func_end, block_refer);
	if (next == NULL) {
	  remove_successor_blocks(blocks, block, block_refer, refer_size);
	  goto failed;
	}
	else
	  set_control_flow(block, next);
      }
#else
      {
	bfd_vma call_target, call_return;
	if (resolve_call(inst, &call_target, &call_return)) {

	  if ((call_target >= func_start && call_target < func_end)) {
	    next = get_basic_blocks(call_target, blocks, grid, func_start, func_end, block_refer);
	    if (next == NULL) {
	      remove_successor_blocks(blocks, block, block_refer, refer_size);
	      goto failed;
	    }
	    else
	      set_control_flow(block, next);
	  }

	  if ((call_return >= func_start && call_return < func_end)) {
	    next = get_basic_blocks(call_return, blocks, grid, func_start, func_end, block_refer);
	    if (next == NULL) {
	      remove_successor_blocks(blocks, block, block_refer, refer_size);
	      goto failed;
	    }
	    else
	      set_control_flow(block, next);
	  }
	}
      }
#endif
      return block;


    case INST_JUMP:
      block->end_addr = inst->address + inst->length;
      block->type = INST_JUMP;

      if ((target >= func_start && target < func_end)) {
 	next = get_basic_blocks(target, blocks, grid, func_start, func_end, block_refer);
	if (next == NULL) {
	  remove_successor_blocks(blocks, block, block_refer, refer_size);
	  goto failed;
	}
	else
	  set_control_flow(block, next);
      }
      return block;
      
    case INST_STOP:
      block->end_addr = inst->address + inst->length;
      block->type = INST_STOP;
      return block;
    }
  }	        

  failed:
    remove_referer_entry(block, block_refer, refer_size);
    remove_block(blocks, block);

    return NULL;
}


static void follow_inst_chain(bfd_vma first, bfd_vma last, unsigned char *grid, bfd_vma func_start, double *prob, unsigned int *chain_length, Instruction *prev, unsigned int *next_inst)
{
  bfd_vma pc, target;
  Instruction *inst;
  double p;

  pc = (first > (last - MAX_INST_LEN)) ? first : (last - MAX_INST_LEN);
    
  for (; pc < last; ++pc) {

    inst = get_inst_of(pc);

    if (inst && 
	grid[pc - func_start] != NO_INST 
	&& (pc + inst->length) <= last) {

      switch (get_control_transfer_type(inst, &target)) {

      case INST_CONT:
      case INST_BRANCH:
	if (pc + inst->length != last)
	  break;
      case INST_CALL:
      case INST_STOP:
      case INST_JUMP:

	p = update_junk_score(inst, prev, prob[last - first], chain_length[last - first]);

	if (p < prob[pc - first]) {
	  prob[pc - first] = p;
	  chain_length[pc - first] = chain_length[last - first] + 1;
	  next_inst[pc - first] = last - pc;
	  follow_inst_chain(first, pc, grid, func_start, prob, chain_length, inst, next_inst);
	}
      }
    }
  }
}	  


static BasicBlock* disassemble_gap(bfd_vma first, bfd_vma last, unsigned char *grid, bfd_vma func_start)
{
  BasicBlock *block;
  Instruction *inst;
  double *prob, pmax;
  unsigned int index, *chain_length, *next_inst;
  bfd_vma bfd_max;

  /* printf("[--> Attempting to fill gap from %lx to %lx <--]\n", first, last); */
  

  prob = (double *) xalloc((last - first + 1), sizeof(double));
  chain_length = (unsigned int *) 
	xalloc((last - first + 1), sizeof(unsigned int));
  next_inst = (unsigned int *) 
	xalloc((last - first + 1), sizeof(unsigned int));

  for (index = 0; index <= last - first; ++index) 
    prob[index] = 1.0;
  for (index = 0; index <= last - first; ++index) 
    chain_length[index] = 0;
  for (index = 0; index <= last - first; ++index) 
    next_inst[index] = 0;

  follow_inst_chain(first, last, grid, func_start, prob, chain_length, NULL, next_inst);

  pmax = 1.0;
  bfd_max = last;
  
  for (index = 0; index < last - first; ++index) {
    if (prob[index] < pmax) {
      bfd_max = first + index;
      pmax = prob[index];
    }

/*     if (prob[index] < 1.0) { */
/*       Instruction *i = get_inst_of(first + index); */
/*       printf("  %lx --> %lx: %g\n", first + index, first + index + i->length, prob[index]); */
/*     } */
  }

  if (bfd_max < last) {
    block = (BasicBlock *) xalloc(1, sizeof(BasicBlock));

    block->start_addr = bfd_max;
    block->end_addr = last;
    block->type = INST_GAP;

    while (bfd_max < last) {
      inst = get_inst_of(bfd_max);
      insert(&block->instructions, inst->address, inst);
      bfd_max += next_inst[bfd_max - first];
    }

    free(prob);
    free(chain_length);
    free(next_inst);
    return block;
  }
  else {
    free(prob);
    free(chain_length);
    free(next_inst);
    return NULL;
  }
}

static void reverse_print_list(Element *list)
{
  Instruction *inst;

  if (list != NULL) { 
    reverse_print_list(list->list_next);
    inst = (Instruction *) list->data;
    print_i386_insn(inst, 1);
  }
}


BasicBlock *sort_blocks(HashTable *blocks)
{
  BasicBlock *block, *b;
  BasicBlock *sorted_blocks = NULL;
  Element *he;

  for (he = blocks->head; he != NULL; he = he->list_next) {

    block = (BasicBlock *) he->data;      

    if (sorted_blocks == NULL || block->start_addr < sorted_blocks->start_addr) {
      block->next = sorted_blocks;
      sorted_blocks = block;
    }
    else {
      for (b = sorted_blocks; b->next != NULL; b = b->next) 
	if (block->start_addr < b->next->start_addr) {
	  block->next = b->next;
	  b->next = block;
	  break;
	}
      
      if (b->next == NULL) { 
	b->next = block;
	block->next = NULL;
      }
    }
  } 
  return sorted_blocks;
}

void print_blocks(HashTable *blocks)
{
  BasicBlock *block;
  BasicBlock *sorted_blocks;
  bfd_vma pc;
  sorted_blocks = sort_blocks(blocks);


  for (block = sorted_blocks; block != NULL; block = block->next) {
      for (pc = block->start_addr; pc < block->end_addr; ) {
		Instruction *curr = get_inst_of(pc);
		print_i386_insn(curr, 0);
		pc += curr->length;
      }
  }

}


/* Disassemble a function */
static void disassemble_i386_function(bfd_vma start_addr, 
									  bfd_vma end_addr,
									  HashTable *symbol_table)
{
  bfd_vma pc, target;
  Instruction *inst;
  int type;
  BasicBlock *block, *sorted_blocks, *init_block, *b;

  Element *qe, *he;
  HashTable blocks;

  BasicBlock **block_refer;
  unsigned char *grid;


  if(hexists(symbol_table, start_addr)){
	asymbol *s = (asymbol *) retrieve(symbol_table, start_addr);
	printf("Disassembling function %s\n", s->name);
  } else {
	printf("Disassembling unknown function\n");
  }
  



  if (end_addr < start_addr) {
    printf("fatal: disassemble_i386_function: addresses do not describe a valid range\n");
    exit(1);
  }

  /* initialize datat structures */
  if (debug)
    printf("\n\n[***] Disassembling function from %lx to %lx [***] \n", (unsigned long) start_addr, (unsigned long) end_addr);
  memset(&blocks, 0, sizeof(HashTable));
  grid = (unsigned char *) xalloc((end_addr - start_addr) + MAX_INST_LEN, sizeof(char));
  block_refer = (BasicBlock **) xalloc((end_addr - start_addr) + MAX_INST_LEN, sizeof(BasicBlock *));


   /* find impossible instruction starts -- start from end and work towards start */
  for (pc = end_addr; pc < (end_addr + MAX_INST_LEN); ++pc) 
    grid[pc - start_addr] = NO_INST;
  grid[end_addr - start_addr] = INST_STOP;

  for (pc = end_addr - 1; pc >= start_addr; --pc) {
    inst = get_inst_of(pc);

    /* cannot disassemble bytes */
    if (inst == NULL) {
      grid[pc - start_addr] = NO_INST;
      continue;
    }
    
    /* this instruction preceeds another impossible instruction */
    if (grid[pc - start_addr + inst->length] & NO_INST) {

      switch (get_control_transfer_type(inst, &target)) {
#ifdef DEFAULT_CALLS
      case INST_BRANCH:
      case INST_CALL:
#endif
      case INST_CONT:
	grid[pc - start_addr] = NO_INST;
      }
    }
  }


  /* find basic blocks using information from local control flow transfers */
  for (pc = start_addr; pc < end_addr; ++pc) {

    inst = get_inst_of(pc);

    /* special case 1: invalid instruction */
    if (inst == NULL)
      continue;


    /* special case 2: start of function is first basic block */
    if (pc == start_addr) {
      get_basic_blocks(pc, &blocks, grid, start_addr, end_addr, block_refer);
      continue;
    }

    /* regular case: check for control flow information */
    switch ((type = get_control_transfer_type(inst, &target))) { 
    case INST_JUMP:
    case INST_CALL:
    case INST_BRANCH:  
      if ((target >= start_addr && target < end_addr)) {
 	get_basic_blocks(pc, &blocks, grid, start_addr, end_addr, block_refer);
      }
      break;
    
/*     case INST_BRANCH:   */
/*       get_basic_blocks(pc, &blocks, grid, start_addr, end_addr, block_refer); */
/*       break; */
    }
  }

  /* printf("before merge\n"); */
  /* print_blocks(&blocks); */
  /* merge redundant blocks and get conflict information  */
  merge_blocks(&blocks); 
  /* printf("after merge\n"); */
  /* print_blocks(&blocks); */


  /* set all blocks valid that are reachable from function start  */
  init_block = (BasicBlock *) retrieve(&blocks, start_addr); 
  if (init_block != NULL) 
    set_reachable(init_block); 

  /* Interactive debugging */
  if (interact) {
    char answer[64], line[1024];
    int fd;
    char template_val[] = "/tmp/graph.XXXXXX";

    answer[0] = '\0';
    printf("Dump block graph (y/n)? ");
    fgets(answer, sizeof(answer) - 1, stdin);
    
    if ((answer[0] == 'y') || (answer[0] == 'Y')) {

      fd = mkstemp(template_val);

      strcpy(line, "digraph function {\n");
      write(fd, line, strlen(line));

      for (he = blocks.head; he != NULL; he = he->list_next) { 
	block = (BasicBlock *) he->data;       

	if (block->valid)
	  sprintf(line, "  Ox%lx [ label = \"%lx\\n%lx\", style = filled ];\n", (unsigned long) block->start_addr, (unsigned long) block->start_addr, (unsigned long) block->end_addr);
	else
	  sprintf(line, "  Ox%lx [ label = \"%lx\\n%lx\" ];\n", (unsigned long) block->start_addr, (unsigned long) block->start_addr, (unsigned long) block->end_addr);
	write(fd, line, strlen(line));

	for (qe = block->cf_down.first; qe != NULL; qe = qe->next) {
	  sprintf(line, "  Ox%lx -> Ox%lx;\n",(unsigned long)  block->start_addr, (unsigned long) qe->addr);
	  write(fd, line, strlen(line));
	}

	for (qe = block->conflicts.first; qe != NULL; qe = qe->next) {
	  sprintf(line, "  Ox%lx -> Ox%lx [ style = dotted, arrowhead = none ];\n", (unsigned long) block->start_addr, (unsigned long) qe->addr);
	  write(fd, line, strlen(line));
	}
      }

      strcpy(line, "}");
      write(fd, line, strlen(line));
      close(fd);

      sprintf(line, "dot -Tps %s > %s.ps", template_val, template_val);
      system(line);

      sprintf(line, "gv %s.ps", template_val);
      system(line);

      sprintf(line, "%s.ps", template_val);
      unlink(line);

      unlink(template_val);
    }
  }

#ifdef STATISTICS
  for (he = blocks.head; he != NULL; he = he->list_next) 
    ibb++;
#endif

  /* resolve conflicts between blocks */
  resolve_conflicts(&blocks); 

#ifdef STATISTICS
  for (he = blocks.head; he != NULL; he = he->list_next) 
    bb++;
#endif

  /* sort basic blocks */
  sorted_blocks = NULL;
  for (he = blocks.head; he != NULL; he = he->list_next) {

    block = (BasicBlock *) he->data;      

    if (sorted_blocks == NULL || block->start_addr < sorted_blocks->start_addr) {
      block->next = sorted_blocks;
      sorted_blocks = block;
    }
    else {
      for (b = sorted_blocks; b->next != NULL; b = b->next) 
	if (block->start_addr < b->next->start_addr) {
	  block->next = b->next;
	  b->next = block;
	  break;
	}
      
      if (b->next == NULL) { 
	b->next = block;
	block->next = NULL;
      }
    }
  } 

  /* attempt to fill "call gaps" */
  for (block = sorted_blocks; block && block->next; block = block->next)
    if (block->end_addr < block->next->start_addr) {
      if (block->type == INST_CALL)
	b = disassemble_gap(block->end_addr, block->next->start_addr, grid, start_addr);
      else
	b = disassemble_gap(block->end_addr, block->next->start_addr, grid, start_addr);
      if (b != NULL) {
	insert(&blocks, b->start_addr, b);
	b->next = block->next;
	block->next = b;
      }
    }
  if (block && block->end_addr < end_addr) {
    b = disassemble_gap(block->end_addr, end_addr, grid, start_addr);
    if (b != NULL) {
      insert(&blocks, b->start_addr, b);
      block->next = b;
    }
  }

  
  for (block = sorted_blocks; block != NULL; block = block->next) {

    if (debug) {
      if (block->type == INST_GAP)
	printf("BLOCK: %lx -- %lx (--> guessed!):\n", (unsigned long)block->start_addr, (unsigned long)block->end_addr);
      else 
	printf("BLOCK: %lx -- %lx:\n", (unsigned long)block->start_addr, (unsigned long)block->end_addr);

      printf("       flows to:\n");
      for (qe = block->cf_down.first; qe != NULL; qe = qe->next)
#ifdef BFD64
	printf("          0x%llx\n", qe->addr);
#else
	printf("          0x%lx\n", qe->addr);
#endif
      printf("       parents are:\n");
      for (qe = block->cf_up.first; qe != NULL; qe = qe->next)
#ifdef BFD64
	printf("          0x%llx\n", qe->addr);
#else
	printf("          0x%lx\n", qe->addr);
#endif
      printf("       conflict(s) with:\n");
      for (qe = block->conflicts.first; qe != NULL; qe = qe->next)
#ifdef BFD64
	printf("          0x%llx\n", qe->addr);
#else
	printf("          0x%lx\n", qe->addr);
#endif
      printf("INSTRUCTIONS:\n");
    }
     
    if (block->type == INST_GAP) {
      reverse_print_list(block->instructions.head);
    }
   else {
      for (pc = block->start_addr; pc < block->end_addr; ) {
		Instruction *curr = get_inst_of(pc);
		print_i386_insn(curr, 0);
		pc += curr->length;
      }
	}

    if (debug)
      printf("\n");
    else {
      if (block->next != NULL && block->end_addr < block->next->start_addr)
	printf(" ...\n");
    }
  }
  
     
  /* clean up */
  {
    Element *e, *ne;

    for (e = blocks.head; e != NULL; ) {
      ne = e->list_next;
      remove_block(&blocks, (BasicBlock *) e->data);
      e = ne;
    }
  }
  free(block_refer);
  free(grid);
}

void init_disasm_info(struct disassemble_info *disasm_info, Segment *segment)
{
  disasm_info->buffer = segment->data;
  disasm_info->buffer_vma = segment->section->vma;
  disasm_info->buffer_length = segment->datasize;
  disasm_info->section = segment->section;
}

HashTable *buildSymbolTable(bfd *abfd)
{
  long storage_needed;
  asymbol **symtbl;
  long number_of_symbols;
  long i;
  HashTable *ret = NULL;

  storage_needed = bfd_get_symtab_upper_bound(abfd);
  /* no symbol table */
  if(storage_needed <= 0){
	return ret;
  }
  // AWH - now xalloc(1, size), not xmalloc(size)
  symtbl = (asymbol **)xalloc(1, storage_needed);
  number_of_symbols = bfd_canonicalize_symtab(abfd, symtbl);

  if(number_of_symbols < 0){
	free(symtbl);
	return ret;
  }
  // AWH - now xalloc(1, size), not xmalloc(size)
  ret = (HashTable *) xalloc(1, sizeof(HashTable));
  memset(ret, 0, sizeof(HashTable));
  for(i = 0; i < number_of_symbols; i++){
	asymbol *sym = symtbl[i];

	if(sym->flags & BSF_FUNCTION)
	  insert(ret, sym->value + sym->section->vma, sym);
  }
  free(symtbl);
  return ret;
}

/* Disassemble the contents of an object file.  */
void disassemble_i386_data (bfd *abfd, int print_insn_digraphs, int interactive)
{
  asection *section;
  unsigned int opb;
  Segment *segment;
  unsigned int cnt;
  bfd_vma pc, target;
  HashTable calls;
  struct disassemble_info disasm_info;
  HashTable *symbol_table;

  symbol_table = buildSymbolTable(abfd);

  segs = NULL;
  interact = interactive;
#ifdef DEOBF_PATTERN
  obf_params = NULL;
#endif

  memset(&calls, 0, sizeof(HashTable));

  disasm_info.buffer = NULL;
  disasm_info.buffer_vma = 0;
  disasm_info.buffer_length = 0;
  opb = bfd_octets_per_byte(abfd);
  disasm_info.octets_per_byte = opb;

  /* for all sections -- load the data and initialize the instruction arrays */
  for (section = abfd->sections; section != (asection *) NULL; section = section->next) {

    Segment *seg, *ts;
    bfd_byte *data = NULL;
    bfd_size_type datasize = 0;
    int is_code;

    if ((section->flags & SEC_LOAD) == 0)
      continue;

    if ((section->flags & SEC_CODE) == 0)
      is_code = 0;
    else
      is_code = 1;

    datasize = bfd_get_section_size_before_reloc (section);
    if (datasize == 0)
      continue;
    data = (bfd_byte *) xalloc ((size_t) datasize, (size_t) sizeof(bfd_byte));
    bfd_get_section_contents (abfd, section, data, 0, datasize);

    seg = (Segment *)xalloc(1, sizeof(Segment));

    seg->data = data;
    seg->datasize = datasize;
    seg->start_addr = section->vma;
    seg->end_addr = section->vma + datasize / opb;
    seg->section = section;
    seg->is_code = is_code;

    if (is_code) {

      seg->status = 
		(unsigned char *) xalloc((seg->end_addr - seg->start_addr),
								 sizeof(unsigned char));
    
      seg->insts = (Instruction **)
		xalloc((seg->end_addr - seg->start_addr), sizeof(Instruction *));
      init_disasm_info(&disasm_info, seg);
      for (pc = seg->start_addr; pc < seg->end_addr; ++pc) {
	Instruction *inst = get_i386_insn(&disasm_info, pc, seg);
	seg->insts[pc - seg->start_addr] = inst;

	if (inst != NULL && get_control_transfer_type(inst, &target) == INST_CALL) 
	  hupdate(&calls, target);
      }
    }

    seg->next = NULL;
    if (segs == NULL)
      segs = seg;
    else {
      ts = segs;
      while (ts->next != NULL)
	ts = ts->next;
      ts->next = seg;
    }
  }

  if (show_asm) {


    printf("show simple disassembler output\n");

    for (segment = segs; segment != NULL; segment = segment->next) {

      if (! segment->is_code)
	continue;


    }
  }
  else {

#ifdef DEOBF_PATTERN
    Element *e;
    unsigned long max_cnt = 0;
    bfd_vma obf_func = 0;

    printf("speculative disassmbler\n");

    /* determine most-called function and attempt de-obfuscation */
    for (e = calls.head; e != NULL; e = e->list_next) {
      if ((unsigned long) e->data > max_cnt) {
	max_cnt = (unsigned long) e->data;
	obf_func = e->addr;
      }
    }

    if (obf_func != 0) {
      obf_params = get_obfuscation_params(obf_func);
      if (obf_params == NULL) 
	printf("warning: failed to extract obfuscation parameters\n");
    }
#endif


    /* find possible function starting points */
    for (segment = segs; segment != NULL; segment = segment->next) {
      unsigned int cnt, state;
      bfd_byte octet;
      
      if (! segment->is_code)
	continue;

      /* 
       *  attempt to locate function prologs:
       *
       *  gcc                       : push %ebp (55), followed by mov %esp,%ebp (89 E5)
       *  gcc (-fomit-frame-pointer): sub $0xxx,%esp (83 EC xx)  
       */
      init_disasm_info(&disasm_info, segment);

      for (cnt = 0, state = 0; cnt < segment->datasize; ++cnt) {
	octet = segment->data[cnt];
	
	switch (state) {
	  
	case 0:
	  switch (octet) {
	  case 0x55:
	    state = 1;
	    break;
/* 	  case 0x83: */
/* 	    state = 3; */
/* 	    break; */
	  }
	  break;
	  
	case 1:
	  switch (octet) {
	  case 0x55:
	    state = 1;
	    break;
/* 	  case 0x83: */
/* 	    state = 3; */
/* 	    break; */
	  case 0x89:
	    state = 2;
	    break;
	  default:
	    state = 0;
	    break;
	  }
	  break;
	  
	case 2:
	  switch (octet) {
	  case 0x55:
	    state = 1;
	    break;
/* 	  case 0x83: */
/* 	    state = 3; */
/* 	  break; */
	  case 0xE5:
	    append(&segment->functions, segment->start_addr + cnt - 2, NULL);
	    state = 4;
	    break;
	  default:
	    state = 0;
	    break;
	  }
	  break;
      
/* 	case 3: */
/* 	  switch (octet) { */
/* 	  case 0x55: */
/* 	    state = 1; */
/* 	    break; */
/* 	  case 0x83: */
/* 	    state = 3; */
/* 	    break; */
/* 	  case 0xEC: */
/* 	    append(&segment->functions, segment->start_addr + cnt - 1, NULL); */
/* 	    state = 4; */
/* 	    break; */
/* 	  default: */
/* 	    state = 0; */
/* 	    break; */
/* 	  } */
/* 	  break; */
	  
	case 4:
	  switch (octet) {
	  case 0xC3:
	    state = 0;
	    break;
	  }
	  break;
	}
      }
    }
 


    /*** speculatively disassemble functions ***/
    {
      bfd_vma fstart, fend;
      
      for (segment = segs; segment != NULL; segment = segment->next) {
	
	if (! segment->is_code)
	  continue;

	if (debug)
#ifdef BFD64
	  printf("Segment: %llx - %llx\n", segment->start_addr, segment->end_addr); 
#else
	  printf("Segment: %lx - %lx\n", segment->start_addr, segment->end_addr); 
#endif
	
	fstart = pop(&segment->functions);
	if (fstart != (bfd_vma) -1) {
	  if (fstart > segment->start_addr)
	    disassemble_i386_function(segment->start_addr, fstart, symbol_table);
	  while ((fend = pop(&segment->functions)) != (bfd_vma) -1) {
	    disassemble_i386_function(fstart, fend, symbol_table);
	    fstart = fend;
	  }
	  disassemble_i386_function(fstart, segment->end_addr, symbol_table);
	}
	else 
	  disassemble_i386_function(segment->start_addr,
								segment->end_addr, symbol_table);
      }
    }
  }

#ifdef STATISTICS
  if (statistics) {
    unsigned long removals = r1 + r2 + r3 + r4 + r5 + r6 + r7 + r8 + r9;
    printf("[STATS] Initial Blocks:   %ld\n" \
	   "[STATS] Final Blocks:     %ld (%s)\n" \
	   "[STATS] Individual Tests: %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", \
	   ibb, bb, (bb == (ibb - removals))?"correct":"inconsistent", r1, r2, r3, r4, r5, r6, r7, r8, r9);
  }
#endif


  /* clean up memory */
  {
    Element *e1, *e2;

    for (segment = segs; segment != NULL; ) {
      Segment *todel;
      
      todel = segment;
      segment = segment->next;
      
      free(todel->data);

      if (todel->is_code) {
	for (cnt = 0; cnt < (todel->end_addr - todel->start_addr); ++cnt) {
	  if (todel->insts[cnt] != NULL) 
	    free(todel->insts[cnt]);
	}
	
	free(todel->insts);
	free(todel->status);
      }
      
      free(todel);
    }

    for (e1 = calls.head; e1 != NULL; ) {
      e2 = e1->list_next;
      free(e1);
      e1 = e2;
    }
    
  }
}



void fatal(const char *fmt, ...)
{
  va_list args;
  char buf[1024];
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf)-1, fmt, args);
  va_end(args);
  buf[1023] = 0;
  fprintf(stderr,"%s", buf);
  exit(-1);
}

#endif