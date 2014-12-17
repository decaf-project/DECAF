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
/*
* Junk instruction detector.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// #include "disasm.h"

InstDigraphs digraphs;

void
inst_digraph_init(InstDigraphs *g)
{
  memset(g, 0, sizeof(*g));
}

void
inst_digraph_destroy(InstDigraphs *g)
{
  int i;
  InstDigraphNode *prev, *cur;

  for (i = 0; i < INST_DIGRAPH_HASH_TABLE_SIZE; i++) {
    /* Free digraph nodes */
    cur = g->digraphs[i];
    while (cur) {
      prev = cur;
      cur = cur->next;
      free(prev);
    }

    /* Free count nodes */
    cur = g->counts[i];
    while (cur) {
      prev = cur;
      cur = cur->next;
      free(prev);
    }
  }
}

unsigned int
inst_digraph_hash(unsigned char *o1, unsigned char *o2)
{
  return ((o1[0] ^ o2[0]) ^ ((o1[1] ^ o2[1]) << 8)) &
    (INST_DIGRAPH_HASH_TABLE_SIZE - 1);
}

unsigned int
inst_count_hash(unsigned char *o)
{
  return ((o[0] ^ o[1]) ^ ((o[1] ^ o[2]) << 8)) &
    (INST_DIGRAPH_HASH_TABLE_SIZE - 1);
}

unsigned int
inst_digraph_node_equals(InstDigraphNode *n,
  unsigned char *o1, unsigned char *o2)
{
  return !memcmp(n->opcode, o1, 3) &&
    !memcmp(n->opcode + 3, o2, 3);
}

unsigned int
inst_count_node_equals(InstDigraphNode *n, unsigned char *o)
{
  return !memcmp(n->opcode, o, 3);
}

void
inst_digraph_incr(InstDigraphs *g, Instruction *i1, Instruction *i2)
{
  InstDigraphNode *cur;
  unsigned int hash = inst_digraph_hash(i1->opcode, i2->opcode);

  /* Search for existing digraph node */
  cur = g->digraphs[hash];
  while (cur) {
    if (inst_digraph_node_equals(cur, i1->opcode, i2->opcode))
      break;
    cur = cur->next;
  }

  if (!cur) {
    /* No node found, create a new one */
    if ((cur = (InstDigraphNode *) malloc(sizeof(*cur))) == NULL)
      fatal("inst_digraph_incr(): Unable to allocate new digraph node\n");

    cur->count = 1;
    memcpy(cur->opcode, i1->opcode, 3);
    memcpy(cur->opcode + 3, i2->opcode, 3);

    cur->next = g->digraphs[hash];
    g->digraphs[hash] = cur;
  } else {
    ++cur->count;
  }

  /* Search for existing count node */
  hash = inst_count_hash(i1->opcode);
  cur = g->counts[hash];
  while (cur) {
    if (inst_count_node_equals(cur, i1->opcode))
      break;
    cur = cur->next;
  }

  if (!cur) {
    /* No node found, create a new one */
    if ((cur = (InstDigraphNode *) malloc(sizeof(*cur))) == NULL)
      fatal("inst_digraph_incr(): Unable to allocate new count node\n");

    cur->count = 1;
    memcpy(cur->opcode, i1->opcode, 3);

    cur->next = g->counts[hash];
    g->counts[hash] = cur;
  } else {
    ++cur->count;
  }

  /* Update global count */
  ++g->count;
}

float
inst_digraph_get(InstDigraphs *g, Instruction *i1, Instruction *i2)
{
  InstDigraphNode *cur;
  unsigned int count, hash = inst_digraph_hash(i1->opcode, i2->opcode);

  /* Get digraph node */
  cur = g->digraphs[hash];
  while (cur) {
    if (inst_digraph_node_equals(cur, i1->opcode, i2->opcode))
      break;
    cur = cur->next;
  }

  if (!cur)
    return 0.0;

  count = cur->count;

  /* Get count node */
  hash = inst_count_hash(i1->opcode);
  cur = g->counts[hash];
  while (cur) {
    if (inst_count_node_equals(cur, i1->opcode))
      break;
    cur = cur->next;
  }

  if (!cur)
    fatal("inst_digraph_get(): Have digraph node but no count node?!\n");

  return count / cur->count;
}

float
inst_count_get(InstDigraphs *g, Instruction *i)
{
  InstDigraphNode *cur;
  unsigned int hash = inst_count_hash(i->opcode);

  /* Get count node */
  cur = g->counts[hash];
  while (cur) {
    if (inst_count_node_equals(cur, i->opcode))
      break;
    cur = cur->next;
  }

  if (!cur)
    return 0.0;

  /*  return cur->count / g->count; */
  return (float) cur->count / (float) g->count;
}

void
inst_digraph_put(InstDigraphs *g, unsigned char *o1,
  unsigned char *o2, unsigned int c)
{
  InstDigraphNode *cur;
  unsigned int hash = inst_digraph_hash(o1, o2);

  /* Find existing node */
  cur = g->digraphs[hash];
  while (cur) {
    if (inst_digraph_node_equals(cur, o1, o2))
      break;
    cur = cur->next;
  }

  if (cur) {
    /* Update existing node */
    cur->count = c;
  } else {
    /* No node found, create a new one */
    if ((cur = (InstDigraphNode *) malloc(sizeof(*cur))) == NULL)
      fatal("inst_digraph_put(): Unable to allocate new digraph node\n");

    cur->count = c;
    memcpy(cur->opcode, o1, 3);
    memcpy(cur->opcode + 3, o2, 3);

    cur->next = g->digraphs[hash];
    g->digraphs[hash] = cur;
  }
}

void
inst_count_put(InstDigraphs *g, unsigned char *o, unsigned int c)
{
  InstDigraphNode *cur;
  unsigned int hash = inst_count_hash(o);

  /* Find existing node */
  cur = g->counts[hash];
  while (cur) {
    if (inst_count_node_equals(cur, o))
      break;
    cur = cur->next;
  }

  if (cur) {
    /* Update existing node */
    cur->count = c;
  } else {
    /* No node found, create a new one */
    if ((cur = (InstDigraphNode *) malloc(sizeof(*cur))) == NULL)
      fatal("inst_count_put(): Unable to allocate new count node\n");

    cur->count = c;
    memcpy(cur->opcode, o, 3);

    cur->next = g->counts[hash];
    g->counts[hash] = cur;
  }
}

void
inst_digraph_parse(InstDigraphs *g, FILE *s)
{
  char buf[1024];
  int i, count, have_global_count = 0, have_inst_counts = 0;
  unsigned char opchars[3], opcodes[6];
  char *dummy = NULL; // AWH - Remove warnings

  /*
   * Expected format is:
   * <global count>
   *
   * <instruction counts>
   * ...
   *
   * <digraph counts>
   * ...
   */

  while (fgets(buf, sizeof(buf), s)) {
#ifdef INST_DIGRAPH_DEBUG
    fprintf(stderr, "DEBUG: line = %s", buf);
#endif

    if (!have_global_count) {
      /* Get global count */
      if (sscanf(buf, "%u", &g->count)) {
#ifdef INST_DIGRAPH_DEBUG
        fprintf(stderr, "DEBUG: global count = %d\n", g->count);
#endif
        have_global_count = 1;
      }

      /* Eat separator line */
      dummy = fgets(buf, sizeof(buf), s);
    } else if (!have_inst_counts) {
      /* Check for instruction, digraph separator line */
      if (!strcmp(buf, "\n")) {
        have_inst_counts = 1;
        continue;
      }

      /* Get instruction opcodes */
      for (i = 0; i < 3; i++) {
        if (sscanf(buf + i * 2, "%2c", opchars) != 1)
          break;
        opchars[sizeof(opchars) - 1] = '\0';
        opcodes[i] = strtoul((char *) opchars, NULL, 16);
      }

      /* Check opcodes, get instruction count */
      if (i < 3 || sscanf(buf + 6, ": %d", &count) != 1)
        continue;

#ifdef INST_DIGRAPH_DEBUG
      fprintf(stderr, "DEBUG: ");
      for (i = 0; i < 3; i++)
        fprintf(stderr, "%02x", opcodes[i]);
      fprintf(stderr, ": %d\n", count);
#endif

      /* Update instruction count */
      inst_count_put(g, opcodes, count);
    } else {
      /* Get digraph opcodes */
      for (i = 0; i < 6; i++) {
        if (sscanf(buf + i * 2, "%2c", opchars) != 1)
          break;
        opchars[sizeof(opchars) - 1] = '\0';
        opcodes[i] = strtoul((char *)opchars, NULL, 16);
      }

      /* Check opcodes, get digraph count */
      if (i < 6 || sscanf(buf + 12, ": %d", &count) != 1)
        continue;

#ifdef INST_DIGRAPH_DEBUG
      fprintf(stderr, "DEBUG: ");
      for (i = 0; i < 6; i++)
        fprintf(stderr, "%02x", opcodes[i]);
      fprintf(stderr, ": %d\n", count);
#endif

      /* Update digraph count */
      inst_digraph_put(g, opcodes, opcodes + 3, count);
    }
  }
}

void
inst_digraph_print(InstDigraphs *g)
{
  int i, j;
  InstDigraphNode *cur;

  fprintf(stderr, "%d\n\n", g->count);

  for (i = 0; i < INST_DIGRAPH_HASH_TABLE_SIZE; i++) {
    cur = g->counts[i];
    while (cur) {
      for (j = 0; j < 3; j++)
        fprintf(stderr, "%02x", cur->opcode[j]);
      fprintf(stderr, ": %d\n", cur->count);
      cur = cur->next;
    }
  }

  fprintf(stderr, "\n");

  for (i = 0; i < INST_DIGRAPH_HASH_TABLE_SIZE; i++) {
    cur = g->digraphs[i];
    while (cur) {
      for (j = 0; j < 6; j++)
        fprintf(stderr, "%02x", cur->opcode[j]);
      fprintf(stderr, ": %d\n", cur->count);
      cur = cur->next;
    }
  }
}

unsigned char
indep_regnum(unsigned char reg)
{
  if (reg >= al_reg && reg <= bh_reg)
    return reg - al_reg;
  else if (reg >= ax_reg && reg <= di_reg)
    return reg - ax_reg;
  else if (reg >= eax_reg && reg <= edi_reg)
    return reg - eax_reg;
  else 
    return 255;
}

unsigned char
reg_width(unsigned char reg)
{
  if (reg >= eax_reg && reg <= edi_reg)
    return d_mode;
  else if (reg >= ax_reg && reg <= di_reg)
    return w_mode;
  else if (reg >= al_reg && reg <= bh_reg)
    return b_mode;
  else
    return 0;
}

unsigned char
op_width(Operand *op)
{
  unsigned char w = 0;

  switch (op->type) {
    case TRegister:
      w = reg_width(op->value.reg.num);
      break;

    case TMemLoc:
      w = d_mode;
      break;

    case TImmediate:
      w = op->value.imm.type;
      break;

    case TJump:
      w = op->value.jmp.type;
      break;

    case TFloatRegister:
      w = reg_width(op->value.freg.num);
      break;

    default:
      fatal("op_width(): Invalid operand type (%#x)\n", op->type);
  }

  return w;
}

unsigned int
is_mov(Instruction *i)
{
  unsigned char c = i->opcode[0];

  if ((c >= 0x88 && c <= 0x8c) ||
    (c == 0x8e) ||
    (c >= 0xa0 && c <= 0xa3) ||
    (c >= 0xb0 && c <= 0xbf) ||
    (c >= 0xc6 && c <= 0xc7))
    return 1;
  return 0;
}

unsigned int
is_mul(Instruction *i)
{
  unsigned char c = i->opcode[0];

  switch (c) {
    default:
      return 0;
  }
}

unsigned int
is_div(Instruction *i)
{
  unsigned char c = i->opcode[0];

  switch (c) {
    default:
      return 0;
  }
}

unsigned int
is_shift(Instruction *i)
{
  unsigned char c = i->opcode[0];

  switch (c) {
    default:
      return 0;
  }
}

unsigned int
is_uncommon_arithmetic(Instruction *i)
{
  if (is_mul(i) || is_div(i) || is_shift(i))
    return 1;
  return 0;
}

unsigned int
is_special_reg(Operand *o)
{
  switch (o->value.reg.num) {
    case es_reg:
    case cs_reg:
    case ss_reg:
    case ds_reg:
    case fs_reg:
    case gs_reg:
    case sp_reg:
    case bp_reg:
    case si_reg:
    case di_reg:
    case esp_reg:
    case ebp_reg:
    case esi_reg:
    case edi_reg:
      return 1;

    default:
      return 0;
  }
}



static int
is_bad_instruction(Instruction *inst)
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

  if (template_val) 
    if (!strcmp(template_val, "(bad)"))
      return 1;
  
  return 0;
}





float
score_junk_insts(Instruction *last, Instruction *cur)
{
  float tmp, score = 0.0;
  Operand *o1, *o2, *o3;
  char m1, m2;

  /* Special case 0x00 opcode and 0x00 mod/rm */
  if (cur->opcode[0] == 0x00 && cur->length == 2 && cur->modrm == 0)
    score += WEIGHT_0x00_OPCODE;

  /* dislike NOPs and (bad) operations */
  if (is_bad_instruction(cur))
    return 1.0;

  if (cur->opcode[0] == 0x90)
    score += WEIGHT_NOP;
  

  /* Check instruction counts */
  tmp = inst_count_get(&digraphs, cur);
  score += (1.0 - tmp) * WEIGHT_INST_COUNT;

  /* Check instruction digraphs */
  if (last)
    score += (1.0 - inst_digraph_get(&digraphs, last, cur)) * WEIGHT_INST_DIGRAPH;

  if (cur->ops[0].type != TNone) {
    /*
     * One operand checks.
     */
    o1 = cur->ops;

    /* Check for unexpected arithmetic on special-purpose registers */
    if (is_uncommon_arithmetic(cur) && is_special_reg(o1))
      score += WEIGHT_ARITHMETIC_SPECIAL_REG;

    if (cur->ops[1].type != TNone) {
      /*
       * Two operand checks.
       */
      o2 = cur->ops + 1;
  
      /* Check for mismatched operand sizes */
      m1 = op_width(o1);
      m2 = op_width(o2);
  
      if (m1 < m2)
        /* w00t */
        score += WEIGHT_INST_OP_SIZE_MISMATCH;
  
      /*
       * Check for moving register contents into memory reference
       * by same register, or vice versa.
       */
      if (o1->type == TRegister && o2->type == TMemLoc) {
        if (indep_regnum(o1->value.reg.num) == indep_regnum(o2->value.mem.base))
          score += WEIGHT_INST_MOV_REG_OWN_REF;
      } else if (o1->type == TMemLoc && o2->type == TRegister) {
        if (indep_regnum(o2->value.reg.num) == indep_regnum(o1->value.mem.base))
          score += WEIGHT_INST_MOV_REG_OWN_REF;
      }

      /*
       * Check for a move from a register other than %ebp into %esp,
       * and vice versa.
       */
      if (is_mov(cur) && o1->type == TRegister && o2->type == TRegister &&
        ((o1->value.reg.num == esp_reg && o2->value.reg.num != ebp_reg) ||
        (o1->value.reg.num == ebp_reg && o2->value.reg.num != esp_reg)))
        score += WEIGHT_INST_MOV_EBP_ESP_MISMATCH;
  
      if (cur->ops[2].type != TNone) {
        /*
         * Three operand checks.
         */
        o3 = cur->ops + 2;
      }
    }
  }

  if (score > 1.0)
    score = 1.0;

  return score;
}

float
update_junk_score(Instruction *newinst, Instruction *next,
  float orig, unsigned int num)
{
  float weight_1, weight_2, cnt, result, score;

  score = score_junk_insts(next, newinst);
  
  cnt = (float) num;
  
  weight_1 = cnt / (cnt + 1.0);
  weight_2 = 1.0 - weight_1;

  result = (weight_1 + weight_2 * score) * orig;

  return result;
  /* return (score_junk_insts(newinst, next) + orig) / (num + 1); */
}

float
detect_junk_insts(bfd_vma start, bfd_vma end)
{
  Instruction *inst = NULL, *last = NULL;
  unsigned int i, num = 0;
  float score = 0.0;
  Segment *segment = get_segment_of(start);

  for (i = start; i < end; i += inst->length, num++) {
    /* Bail if instruction hasn't been decoded */
    if (!segment->insts[i - segment->start_addr])
      break;

    /* Get instruction */
    inst = segment->insts[i - segment->start_addr];

    /* Update anomaly score */
    score += score_junk_insts(last, inst);

    last = inst;
  }

  return score / num;
}

