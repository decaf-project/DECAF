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
#include "disasm-pp.h"

void
ostream_i386_register(int regnum, ostream &out)
{
  if (regnum >= eax_reg)
    out << "%" << intel_names32[regnum-eax_reg];
  else if (regnum >= ax_reg)
    out << "%" << intel_names16[regnum- ax_reg];
  else if (regnum >= al_reg)
    out << "%" << intel_names8[regnum- al_reg];
  else if (regnum >= es_reg)
    out << "%" << intel_names_seg[regnum- es_reg];
  else 
    out << "[unknown register]";
}


void
ostream_i386_mnemonic(Instruction *inst, ostream &out)
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
	out << *template_val;
      else {
	int i;

	switch (*template_val) {
	case 'A':
	  for (i = 0; i < 3; ++i)
	    if (inst->ops[0].type == TRegister)
	      break;
	  if (i == 3)
	    out << "b";
	  break;

	case 'P':
	  if (inst->prefixes & PREFIX_OP)
	    out << "l";
	  break;

	case 'Q':
	  for (i = 0; i < 3; ++i)
	    if (inst->ops[0].type == TRegister)
	      break;
	  if (i == 3) {
	    if (inst->prefixes & PREFIX_OP)
	      out << "w";
	    else
	      out << "l";
	  }
	  break;
	  
	case 'E':
	  if (!(inst->prefixes & PREFIX_OP))
	    out << "e";
	  break;

	case 'R':
	case 'F':
	  if (inst->prefixes & PREFIX_OP)
	    out << "w";
	  else
	    out << "l";
	  break;

	case 'W':
	  if (inst->prefixes & PREFIX_OP)
	    out << "b";
	  else
	    out << "w";
	  break;

	case 'N':
	  if (!(inst->prefixes & PREFIX_FWAIT))
	    out << "n";
	  break;

	}
      }
    }
    
    /* branch prediction */
    if (inst->prefixes & PREFIX_NOT_TAKEN)
      out << ",pn";
    if (inst->prefixes & PREFIX_TAKEN)
      out << ",p";

    //   out << "\t";
  }
}


void ostream_i386_insn(Instruction *inst, ostream &out)

{
  int index, needcomma = 0;

  if (inst == NULL) {
    out << "(null)";
    return;
  }

  /* print address */
  out << hex << inst->address << ":\t";

  /* print prefixes */
  if (inst->prefixes & PREFIX_REPZ)
    out << "repz ";
  if (inst->prefixes & PREFIX_REPNZ)
    out << "repnz ";


  /* print mnemonic */
  ostream_i386_mnemonic(inst, out);
  out << "\t";
  for (index = 2; index >= 0; --index) {

    switch (inst->ops[index].type) {

    case TRegister:
      if (needcomma)
	out << ",";
      if (inst->ops[index].indirect)
	out << "*";
      ostream_i386_register(inst->ops[index].value.reg.num, out);
      needcomma = 1;
      break;

    case TMemAddress: /* TMemAddress was formerly used for LEA, etc. */
    case TMemLoc:
      if (needcomma)
		out << ",";
      if (inst->ops[index].indirect)
		out << "*";
      if (inst->ops[index].value.mem.displ_t)
		out << hex << inst->ops[index].value.mem.displ;
      if (inst->ops[index].value.mem.segment) {
		ostream_i386_register(inst->ops[index].value.mem.segment, out);
		out << ":";
      }
      if (inst->ops[index].value.mem.base || inst->ops[index].value.mem.index)
		out << "(";
      if (inst->ops[index].value.mem.base)
		ostream_i386_register(inst->ops[index].value.mem.base, out);
      if (inst->ops[index].value.mem.index) {
		out << ",";
		ostream_i386_register(inst->ops[index].value.mem.index, out);
      }
      if (inst->ops[index].value.mem.scale){
		int scale = (int) inst->ops[index].value.mem.scale;
		out << "," <<scale ;
      }
      if (inst->ops[index].value.mem.base || inst->ops[index].value.mem.index)
		out << ")";
      needcomma = 1;
      break;

    case TImmediate:
      if (needcomma)
	out << ",";
      if (inst->ops[index].value.imm.segment)
	out << "$0x" << hex << inst->ops[index].value.imm.segment;
      out << "$0x" << hex <<
	inst->ops[index].value.imm.value;
      needcomma = 1;
      break;

    case TJump:
      if (needcomma)
	out << ",";
      out << "0x" << hex <<
	inst->ops[index].value.jmp.target;
      needcomma = 1;
      break;

    case TFloatRegister:
      if (needcomma)
	out << ",";
      if (inst->ops[index].value.freg.def)
	out << "%st";
      else
	out << "%st(" << (int)inst->ops[index].value.freg.num << ")";
      needcomma = 1;
      break;

    case TNone:
      break;
    }
  }
  //  out << endl;
}



