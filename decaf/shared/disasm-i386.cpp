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
/*                Recursive i386 Disassembler Opcodes                         */
/******************************************************************************/

// #include "disasm.h"

// AWH - Moved these here from disasm.h because of a conflict with target-i386/cpu.h
#define ST OP_ST, 0
#define STi OP_STi, 0

const unsigned char onebyte_has_modrm[256] = {
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
  /*       -------------------------------        */
  /* 00 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 00 */
  /* 10 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 10 */
  /* 20 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 20 */
  /* 30 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 30 */
  /* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
  /* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
  /* 60 */ 0,0,1,1,0,0,0,0,0,1,0,1,0,0,0,0, /* 60 */
  /* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
  /* 80 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 80 */
  /* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
  /* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* a0 */
  /* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* b0 */
  /* c0 */ 1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0, /* c0 */
  /* d0 */ 1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1, /* d0 */
  /* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* e0 */
  /* f0 */ 0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1  /* f0 */
  /*       -------------------------------        */
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

const unsigned char onebyte_is_group[256] = {
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
  /*       -------------------------------        */
  /* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 00 */
  /* 10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 10 */
  /* 20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 20 */
  /* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 30 */
  /* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
  /* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
  /* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60 */
  /* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
  /* 80 */ 1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0, /* 80 */
  /* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
  /* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* a0 */
  /* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* b0 */
  /* c0 */ 1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* c0 */
  /* d0 */ 1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0, /* d0 */
  /* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* e0 */
  /* f0 */ 0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1  /* f0 */
  /*       -------------------------------        */
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};




const unsigned char twobyte_has_modrm[256] = {
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
  /*       -------------------------------        */
  /* 00 */ 1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1, /* 0f */
  /* 10 */ 1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0, /* 1f */
  /* 20 */ 1,1,1,1,1,0,1,0,1,1,1,1,1,1,1,1, /* 2f */
  /* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
  /* 40 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 4f */
  /* 50 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 5f */
  /* 60 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 6f */
  /* 70 */ 1,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1, /* 7f */
  /* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
  /* 90 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 9f */
  /* a0 */ 0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1, /* af */
  /* b0 */ 1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1, /* bf */
  /* c0 */ 1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0, /* cf */
  /* d0 */ 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* df */
  /* e0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ef */
  /* f0 */ 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0  /* ff */
  /*       -------------------------------        */
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

const unsigned char twobyte_is_group[256] = {
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
  /*       -------------------------------        */
  /* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 00 */
  /* 10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 10 */
  /* 20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 20 */
  /* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 30 */
  /* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
  /* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
  /* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60 */
  /* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
  /* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 80 */
  /* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
  /* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* a0 */
  /* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* b0 */
  /* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* c0 */
  /* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* d0 */
  /* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* e0 */
  /* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* f0 */
  /*       -------------------------------        */
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

const unsigned char twobyte_uses_SSE_prefix[256] = {
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
  /*       -------------------------------        */
  /* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
  /* 10 */ 1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 1f */
  /* 20 */ 0,0,0,0,0,0,0,0,0,0,1,0,1,1,0,0, /* 2f */
  /* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
  /* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
  /* 50 */ 0,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1, /* 5f */
  /* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1, /* 6f */
  /* 70 */ 1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1, /* 7f */
  /* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
  /* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
  /* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
  /* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
  /* c0 */ 0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
  /* d0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* df */
  /* e0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* ef */
  /* f0 */ 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0  /* ff */
  /*       -------------------------------        */
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

const char *intel_names32[] = {
  "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
};
const char *intel_names16[] = {
  "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
};
const char *intel_names8[] = {
  "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
};
const char *intel_names_seg[] = {
  "es", "cs", "ss", "ds", "fs", "gs", "?", "?",
};

const char *intel_index16[] = {
  "bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "bp", "bx"
};

struct i386_op i386_op_1[] = {
  /* 00 */
  { "add",		Eb, Gb, XX },
  { "add",		Ev, Gv, XX },
  { "add",		Gb, Eb, XX },
  { "add",		Gv, Ev, XX },
  { "add",		AL, Ib, XX },
  { "add",		eAX, Iv, XX },
  { "pushP",		ES, XX, XX },
  { "popP",		ES, XX, XX },
  /* 08 */
  { "or",		Eb, Gb, XX },
  { "or",		Ev, Gv, XX },
  { "or",		Gb, Eb, XX },
  { "or",		Gv, Ev, XX },
  { "or",		AL, Ib, XX },
  { "or",		eAX, Iv, XX },
  { "pushP",		CS, XX, XX },
  { "(bad)",		XX, XX, XX },	/* 0x0f extended opcode escape */
  /* 10 */
  { "adc",		Eb, Gb, XX },
  { "adc",		Ev, Gv, XX },
  { "adc",		Gb, Eb, XX },
  { "adc",		Gv, Ev, XX },
  { "adc",		AL, Ib, XX },
  { "adc",		eAX, Iv, XX },
  { "pushP",		SS, XX, XX },
  { "popP",		SS, XX, XX },
  /* 18 */
  { "sbb",		Eb, Gb, XX },
  { "sbb",		Ev, Gv, XX },
  { "sbb",		Gb, Eb, XX },
  { "sbb",		Gv, Ev, XX },
  { "sbb",		AL, Ib, XX },
  { "sbb",		eAX, Iv, XX },
  { "pushP",		DS, XX, XX },
  { "popP",		DS, XX, XX },
  /* 20 */
  { "and",		Eb, Gb, XX },
  { "and",		Ev, Gv, XX },
  { "and",		Gb, Eb, XX },
  { "and",		Gv, Ev, XX },
  { "and",		AL, Ib, XX },
  { "and",		eAX, Iv, XX },
  { "(bad)",		XX, XX, XX },	/* SEG ES prefix */
  { "daa",		XX, XX, XX },
  /* 28 */
  { "sub",		Eb, Gb, XX },
  { "sub",		Ev, Gv, XX },
  { "sub",		Gb, Eb, XX },
  { "sub",		Gv, Ev, XX },
  { "sub",		AL, Ib, XX },
  { "sub",		eAX, Iv, XX },
  { "(bad)",		XX, XX, XX },	/* SEG CS prefix */
  { "das",		XX, XX, XX },
  /* 30 */
  { "xor",		Eb, Gb, XX },
  { "xor",		Ev, Gv, XX },
  { "xor",		Gb, Eb, XX },
  { "xor",		Gv, Ev, XX },
  { "xor",		AL, Ib, XX },
  { "xor",		eAX, Iv, XX },
  { "(bad)",		XX, XX, XX },	/* SEG SS prefix */
  { "aaa",		XX, XX, XX },
  /* 38 */
  { "cmp",		Eb, Gb, XX },
  { "cmp",		Ev, Gv, XX },
  { "cmp",		Gb, Eb, XX },
  { "cmp",		Gv, Ev, XX },
  { "cmp",		AL, Ib, XX },
  { "cmp",		eAX, Iv, XX },
  { "(bad)",		XX, XX, XX },	/* SEG DS prefix */
  { "aas",		XX, XX, XX },
  /* 40 */
  { "inc",		eAX, XX, XX },
  { "inc",		eCX, XX, XX },
  { "inc",		eDX, XX, XX },
  { "inc",		eBX, XX, XX },
  { "inc",		eSP, XX, XX },
  { "inc",		eBP, XX, XX },
  { "inc",		eSI, XX, XX },
  { "inc",		eDI, XX, XX },
  /* 48 */
  { "dec",		eAX, XX, XX },
  { "dec",		eCX, XX, XX },
  { "dec",		eDX, XX, XX },
  { "dec",		eBX, XX, XX },
  { "dec",		eSP, XX, XX },
  { "dec",		eBP, XX, XX },
  { "dec",		eSI, XX, XX },
  { "dec",		eDI, XX, XX },
  /* 50 */
  { "push",		eAX, XX, XX },
  { "push",		eCX, XX, XX },
  { "push",		eDX, XX, XX },
  { "push",		eBX, XX, XX },
  { "push",		eSP, XX, XX },
  { "push",		eBP, XX, XX },
  { "push",		eSI, XX, XX },
  { "push",		eDI, XX, XX },
  /* 58 */
  { "pop",		eAX, XX, XX },
  { "pop",		eCX, XX, XX },
  { "pop",		eDX, XX, XX },
  { "pop",		eBX, XX, XX },
  { "pop",		eSP, XX, XX },
  { "pop",		eBP, XX, XX },
  { "pop",		eSI, XX, XX },
  { "pop",		eDI, XX, XX },
  /* 60 */
  { "pushaP",     	XX, XX, XX },
  { "popaP",		XX, XX, XX },
  { "bound",	        Gv, Ma, XX },
  { "(not implemented)",XX, XX, XX },   /* 64-bit special */
  { "(bad)",		XX, XX, XX },	/* seg fs */
  { "(bad)",		XX, XX, XX },	/* seg gs */
  { "(bad)",		XX, XX, XX },	/* op size prefix */
  { "(bad)",		XX, XX, XX },	/* adr size prefix */
  /* 68 */
  { "pushP",            Iv, XX, XX },   
  { "imul",		Gv, Ev, Iv },
  { "pushP",		sIb, XX, XX },
  { "imul",		Gv, Ev, sIb },
  { "insb",	        Yb, indirDX, XX },
  //{ "(not implemented)", Yb, indirDX, XX }, //changed by Heng Yin
  { "insR",	        Yv, indirDX, XX },
  //{ "(not implemented)", Yv, indirDX, XX }, //changed by Heng Yin
  { "outsb",	        indirDX, Xb, XX },
  { "outsR",	        indirDX, Xv, XX },
  /* 70 */
  { "jo",		Jb, XX, XX },
  { "jno",		Jb, XX, XX },
  { "jb",		Jb, XX, XX },
  { "jae",		Jb, XX, XX },
  { "je",		Jb, XX, XX },
  { "jne",		Jb, XX, XX },
  { "jbe",		Jb, XX, XX },
  { "ja",		Jb, XX, XX },
  /* 78 */
  { "js",		Jb, XX, XX },
  { "jns",		Jb, XX, XX },
  { "jp",		Jb, XX, XX },
  { "jnp",		Jb, XX, XX },
  { "jl",		Jb, XX, XX },
  { "jge",		Jb, XX, XX },
  { "jle",		Jb, XX, XX },
  { "jg",		Jb, XX, XX },
  /* 80 */
  { "[Group: 80]",      GRP0, XX, XX },   /* Groups */
  { "[Group: 81]",      GRP1, XX, XX },   /* Groups */
  { "[Group: 82]",      GRP2, XX, XX },   /* Groups */
  { "[Group: 83]",      GRP3, XX, XX },   /* Groups */
  { "test",		Eb, Gb, XX },
  { "test",		Ev, Gv, XX },
  { "xchg",		Eb, Gb, XX },
  { "xchg",		Ev, Gv, XX },
  /* 88 */
  { "mov",		Eb, Gb, XX },
  { "mov",		Ev, Gv, XX },
  { "mov",		Gb, Eb, XX },
  { "mov",		Gv, Ev, XX },
  { "movQ",		Ev, Sw, XX },
  { "lea",		Gv, M, XX },
  { "movQ",		Sw, Ev, XX },
  { "popQ",		Ev, XX, XX },
  /* 90 */
  { "nop",		XX, XX, XX },
  { "xchg",		eCX, eAX, XX },
  { "xchg",		eDX, eAX, XX },
  { "xchg",		eBX, eAX, XX },
  { "xchg",		eSP, eAX, XX },
  { "xchg",		eBP, eAX, XX },
  { "xchg",		eSI, eAX, XX },
  { "xchg",		eDI, eAX, XX },
  /* 98 */
  { "cWtR", 	        XX, XX, XX },
  { "cRtd", 	        XX, XX, XX },
  { "lcallP",	        Ap, XX, XX },
  { "(bad)",		XX, XX, XX },	/* fwait */
  { "pushfP",		XX, XX, XX },
  { "popfP",		XX, XX, XX },
  { "sahf",		XX, XX, XX },
  { "lahf",		XX, XX, XX },
  /* a0 */
  { "mov",		AL, Ob64, XX },
  { "mov",		eAX, Ov64, XX },
  { "mov",		Ob64, AL, XX },
  { "mov",		Ov64, eAX, XX },
  { "movsb",	        Yb, Xb, XX },
  { "movsR",	        Yv, Xv, XX },
  { "cmpsb",	        Xb, Yb, XX },
  { "cmpsR",	        Xv, Yv, XX },
  /* a8 */
  { "test",		AL, Ib, XX },
  { "test",		eAX, Iv, XX },
  { "stos",		Yb, AL, XX },
  { "stos",		Yv, eAX, XX },
  { "lods",		AL, Xb, XX },
  { "lods",		eAX, Xv, XX },
  { "scas",		AL, Yb, XX },
  { "scas",		eAX, Yv, XX },
  /* b0 */
  { "mov",		AL, Ib, XX },
  { "mov",		CL, Ib, XX },
  { "mov",		DL, Ib, XX },
  { "mov",		BL, Ib, XX },
  { "mov",		AH, Ib, XX },
  { "mov",		CH, Ib, XX },
  { "mov",		DH, Ib, XX },
  { "mov",		BH, Ib, XX },
  /* b8 */
  { "mov",		eAX, Iv, XX },
  { "mov",		eCX, Iv, XX },
  { "mov",		eDX, Iv, XX },
  { "mov",		eBX, Iv, XX },
  { "mov",		eSP, Iv, XX },
  { "mov",		eBP, Iv, XX },
  { "mov",		eSI, Iv, XX },
  { "mov",		eDI, Iv, XX },
  /* c0 */
  { "[Group: C0]",      GRP4, XX, XX },   /* Groups */
  { "[Group: C1]",      GRP5, XX, XX },   /* Groups */
  { "retP",		Iw, XX, XX },
  { "retP",		XX, XX, XX },
  { "les",		Gv, Mp, XX },
  { "lds",		Gv, Mp, XX },
  { "movA",		Eb, Ib, XX },
  { "movQ",		Ev, Iv, XX },
  /* c8 */
  { "enterP",		Iw, Ib, XX },
  { "leaveP",		XX, XX, XX },
  { "lretP",		Iw, XX, XX },
  { "lretP",		XX, XX, XX },
  { "int3",		XX, XX, XX },
  { "int",		Ib, XX, XX },
  { "into",		XX, XX, XX },
  { "iretP",		XX, XX, XX },
  /* d0 */
  { "[Group: D0]",      GRP6, XX, XX },   /* Groups */
  { "[Group: D1]",      GRP7, XX, XX },   /* Groups */
  { "[Group: D2]",      GRP8, XX, XX },   /* Groups */
  { "[Group: D3]",      GRP9, XX, XX },   /* Groups */
  { "aam",		sIb, XX, XX },
  { "aad",		sIb, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "xlat",		DSBX, XX, XX },
  /* d8 */
/* FLOATing point operations */
  { "[ float ]",        XX, XX, XX },
  { "[ float ]",        XX, XX, XX },
  { "[ float ]",        XX, XX, XX },
  { "[ float ]",        XX, XX, XX },
  { "[ float ]",        XX, XX, XX },
  { "[ float ]",        XX, XX, XX },
  { "[ float ]",        XX, XX, XX },
  { "[ float ]",        XX, XX, XX },
  /* e0 */
  { "loopneF",		Jb, XX, XX },
  { "loopeF",		Jb, XX, XX },
  { "loopF",		Jb, XX, XX },
  { "jEcxz",		Jb, XX, XX },
  { "in",		AL, Ib, XX },
  { "in",		eAX, Ib, XX },
  { "out",		Ib, AL, XX },
  { "out",		Ib, eAX, XX },
  /* e8 */
  { "callP",		Jv, XX, XX },
  { "jmpP",		Jv, XX, XX },
  { "ljmpP",		Ap, XX, XX },
  { "jmp",		Jb, XX, XX },
  { "in",		AL, indirDX, XX },
  { "in",		eAX, indirDX, XX },
  { "out",		indirDX, AL, XX },
  { "out",		indirDX, eAX, XX },
  /* f0 */
  { "(bad)",		XX, XX, XX },	/* lock prefix */
  { "icebp",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },	/* repne */
  { "(bad)",		XX, XX, XX },	/* repz */
  { "hlt",		XX, XX, XX },
  { "cmc",		XX, XX, XX },
  { "[Group: F6]",      GRP10, XX, XX },   /* Groups */
  { "[Group: F7]",      GRP11, XX, XX },   /* Groups */
  /* f8 */
  { "clc",		XX, XX, XX },
  { "stc",		XX, XX, XX },
  { "cli",		XX, XX, XX },
  { "sti",		XX, XX, XX },
  { "cld",		XX, XX, XX },
  { "std",		XX, XX, XX },
  { "[Group: FE]",      GRP12, XX, XX },   /* Groups */
  { "[Group: FF]",      GRP13, XX, XX },   /* Groups */
};


// This is copied from binutils/opcodes/i386-dis.c 
// Note that we comment out all group information
  /* static const struct dis386 dis386_twobyte[] = { */
struct i386_op i386_op_2[] = {
  /* 00 */
  //  { GRP6 },
  { "not implemented",  XX, XX, XX },
  //  { GRP7 },
  { "not implemented",  XX, XX, XX },
  { "larS",		Gv, Ew, XX },
  { "lslS",		Gv, Ew, XX },
  { "(bad)",		XX, XX, XX },
  { "syscall",		XX, XX, XX },
  { "clts",		XX, XX, XX },
  { "sysretP",		XX, XX, XX },
  /* 08 */
  { "invd",		XX, XX, XX },
  { "wbinvd",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "ud2a",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  //  { GRPAMD },
  { "not implemented",  XX, XX, XX },
  { "femms",		XX, XX, XX },
  //  { "",			MX, EM, OPSUF }, /* See OP_3DNowSuffix.  */
  { "not implemented",  XX, XX, XX },
  /* 10 */
  //  { PREGRP8 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP9 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP30 },
  { "not implemented",  XX, XX, XX },
  //  { "movlpX",		EX, XM, SIMD_Fixup, 'h' },
  { "not implemented",  XX, XX, XX },
  //{ "unpcklpX",		XM, EX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "unpckhpX",		XM, EX, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP31 },
  { "not implemented",  XX, XX, XX },
  //  { "movhpX",		EX, XM, SIMD_Fixup, 'l' },
  { "not implemented",  XX, XX, XX },
  /* 18 */
  //  { GRP14 },
  { "not implemented",  XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  /* 20 */
  //  { "movZ",		Rm, Cm, XX },
  { "not implemented",  XX, XX, XX },
  //  { "movZ",		Rm, Dm, XX },
  { "not implemented",  XX, XX, XX },
  //  { "movZ",		Cm, Rm, XX },
  { "not implemented",  XX, XX, XX },
  //  { "movZ",		Dm, Rm, XX }
  { "not implemented",  XX, XX, XX },
  //  { "movL",		Rd, Td, XX },
  { "not implemented",  XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  //  { "movL",		Td, Rd, XX },
  { "not implemented",  XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  /* 28 */
  //  { "movapX",		XM, EX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "movapX",		EX, XM, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP2 },
  { "not implemented",  XX, XX, XX },
  //  { "movntpX",		Ev, XM, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP4 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP3 },
  { "not implemented",  XX, XX, XX },
  //  { "ucomisX",		XM,EX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "comisX",		XM,EX, XX },
  { "not implemented",  XX, XX, XX },
  /* 30 */
  { "wrmsr",		XX, XX, XX },
  { "rdtsc",		XX, XX, XX },
  { "rdmsr",		XX, XX, XX },
  { "rdpmc",		XX, XX, XX },
  { "sysenter",		XX, XX, XX },
  { "sysexit",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  /* 38 */
  //  { THREE_BYTE_0 },
  { "not implemented",  XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  //  { THREE_BYTE_1 },
  { "not implemented",  XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  /* 40 */
  { "cmovo",		Gv, Ev, XX },
  { "cmovno",		Gv, Ev, XX },
  { "cmovb",		Gv, Ev, XX },
  { "cmovae",		Gv, Ev, XX },
  { "cmove",		Gv, Ev, XX },
  { "cmovne",		Gv, Ev, XX },
  { "cmovbe",		Gv, Ev, XX },
  { "cmova",		Gv, Ev, XX },
  /* 48 */
  { "cmovs",		Gv, Ev, XX },
  { "cmovns",		Gv, Ev, XX },
  { "cmovp",		Gv, Ev, XX },
  { "cmovnp",		Gv, Ev, XX },
  { "cmovl",		Gv, Ev, XX },
  { "cmovge",		Gv, Ev, XX },
  { "cmovle",		Gv, Ev, XX },
  { "cmovg",		Gv, Ev, XX },
  /* 50 */
  //  { "movmskpX",		Gdq, XS, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP13 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP12 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP11 },
  { "not implemented",  XX, XX, XX },
  //  { "andpX",		XM, EX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "andnpX",		XM, EX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "orpX",		XM, EX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "xorpX",		XM, EX, XX },
  { "not implemented",  XX, XX, XX },
  /* 58 */
  //  { PREGRP0 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP10 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP17 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP16 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP14 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP7 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP5 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP6 },
  { "not implemented",  XX, XX, XX },
  /* 60 */
  //  { "punpcklbw",	MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "punpcklwd",	MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "punpckldq",	MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "packsswb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pcmpgtb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pcmpgtw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pcmpgtd",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "packuswb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  /* 68 */
  //  { "punpckhbw",	MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "punpckhwd",	MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "punpckhdq",	MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "packssdw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP26 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP24 },
  { "not implemented",  XX, XX, XX },
  //  { "movd",		MX, Edq, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP19 },
  { "not implemented",  XX, XX, XX },
  /* 70 */
  //  { PREGRP22 },
  { "not implemented",  XX, XX, XX },
  //  { GRP10 },
  { "not implemented",  XX, XX, XX },
  //  { GRP11 },
  { "not implemented",  XX, XX, XX },
  //  { GRP12 },
  { "not implemented",  XX, XX, XX },
  //  { "pcmpeqb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pcmpeqw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pcmpeqd",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  { "emms",		XX, XX, XX },
  /* 78 */
  //  { "vmread",		Em, Gm, XX },
  { "not implemented",  XX, XX, XX },
  //  { "vmwrite",		Gm, Em, XX },
  { "not implemented",  XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  { "(bad)",		XX, XX, XX },
  //  { PREGRP28 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP29 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP23 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP20 },
  { "not implemented",  XX, XX, XX },
  /* 80 */
  //{ "joH",		Jv, XX, cond_jump_flag },
  //{ "jnoH",		Jv, XX, cond_jump_flag },
  //{ "jbH",		Jv, XX, cond_jump_flag },
  //{ "jaeH",		Jv, XX, cond_jump_flag },
  //{ "jeH",		Jv, XX, cond_jump_flag },
  //{ "jneH",		Jv, XX, cond_jump_flag },
  //{ "jbeH",		Jv, XX, cond_jump_flag },
  //{ "jaH",		Jv, XX, cond_jump_flag },
  { "joH",		Jv, XX, XX },
  { "jnoH",		Jv, XX, XX },
  { "jbH",		Jv, XX, XX },
  { "jaeH",		Jv, XX, XX },
  { "jeH",		Jv, XX, XX },
  { "jneH",		Jv, XX, XX },
  { "jbeH",		Jv, XX, XX },
  { "jaH",		Jv, XX, XX },
  /* 88 */
  //  { "jsH",		Jv, XX, cond_jump_flag },
  //  { "jnsH",		Jv, XX, cond_jump_flag },
  //  { "jpH",		Jv, XX, cond_jump_flag },
  //  { "jnpH",		Jv, XX, cond_jump_flag },
  //  { "jlH",		Jv, XX, cond_jump_flag },
  //  { "jgeH",		Jv, XX, cond_jump_flag },
  //  { "jleH",		Jv, XX, cond_jump_flag },
  //  { "jgH",		Jv, XX, cond_jump_flag },
  { "jsH",		Jv, XX, XX },
  { "jnsH",		Jv, XX, XX },
  { "jpH",		Jv, XX, XX },
  { "jnpH",		Jv, XX, XX },
  { "jlH",		Jv, XX, XX },
  { "jgeH",		Jv, XX, XX },
  { "jleH",		Jv, XX, XX },
  { "jgH",		Jv, XX, XX },
  /* 90 */
  { "seto",		Eb, XX, XX },
  { "setno",		Eb, XX, XX },
  { "setb",		Eb, XX, XX },
  { "setae",		Eb, XX, XX },
  { "sete",		Eb, XX, XX },
  { "setne",		Eb, XX, XX },
  { "setbe",		Eb, XX, XX },
  { "seta",		Eb, XX, XX },
  /* 98 */
  { "sets",		Eb, XX, XX },
  { "setns",		Eb, XX, XX },
  { "setp",		Eb, XX, XX },
  { "setnp",		Eb, XX, XX },
  { "setl",		Eb, XX, XX },
  { "setge",		Eb, XX, XX },
  { "setle",		Eb, XX, XX },
  { "setg",		Eb, XX, XX },
  /* a0 */
  //  { "pushT",		fs, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "popT",		fs, XX, XX },
  { "not implemented",  XX, XX, XX },
  { "cpuid",		XX, XX, XX },
  { "btS",		Ev, Gv, XX },
  { "shldS",		Ev, Gv, Ib },
  { "shldS",		Ev, Gv, CL },
  //  { GRPPADLCK2 },
  { "not implemented",  XX, XX, XX },
  //  { GRPPADLCK1 },
  { "not implemented",  XX, XX, XX },
  /* a8 */
  //  { "pushT",		gs, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "popT",		gs, XX, XX },
  { "not implemented",  XX, XX, XX },
  { "rsm",		XX, XX, XX },
  { "btsS",		Ev, Gv, XX },
  { "shrdS",		Ev, Gv, Ib },
  { "shrdS",		Ev, Gv, CL },
  //  { GRP13 },
  { "not implemented",  XX, XX, XX },
  { "imulS",		Gv, Ev, XX },
  /* b0 */
  { "cmpxchgB",		Eb, Gb, XX },
  { "cmpxchgS",		Ev, Gv, XX },
  { "lssS",		Gv, Mp, XX },
  { "btrS",		Ev, Gv, XX },
  { "lfsS",		Gv, Mp, XX },
  { "lgsS",		Gv, Mp, XX },
  { "movz{bR|x|bR|x}",	Gv, Eb, XX },
  { "movz{wR|x|wR|x}",	Gv, Ew, XX }, /* yes, there really is movzww ! */
  /* b8 */
  { "(bad)",		XX, XX, XX },
  { "ud2b",		XX, XX, XX },
  //  { GRP8 },
  { "not implemented",  XX, XX, XX },
  { "btcS",		Ev, Gv, XX },
  { "bsfS",		Gv, Ev, XX },
  { "bsrS",		Gv, Ev, XX },
  { "movs{bR|x|bR|x}",	Gv, Eb, XX },
  { "movs{wR|x|wR|x}",	Gv, Ew, XX }, /* yes, there really is movsww ! */
  /* c0 */
  { "xaddB",		Eb, Gb, XX },
  { "xaddS",		Ev, Gv, XX },
  //  { PREGRP1 },
  { "not implemented",  XX, XX, XX },
  { "movntiS",		Ev, Gv, XX },
  //  { "pinsrw",		MX, Edqw, Ib },
  { "not implemented",  XX, XX, XX },
  //  { "pextrw",		Gdq, MS, Ib },
  { "not implemented",  XX, XX, XX },
  //  { "shufpX",		XM, EX, Ib },
  { "not implemented",  XX, XX, XX },
  //  { GRP9 },
  { "not implemented",  XX, XX, XX },
  /* c8 */
  //  { "bswap",		RMeAX, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "bswap",		RMeCX, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "bswap",		RMeDX, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "bswap",		RMeBX, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "bswap",		RMeSP, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "bswap",		RMeBP, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "bswap",		RMeSI, XX, XX },
  { "not implemented",  XX, XX, XX },
  //  { "bswap",		RMeDI, XX, XX },
  { "not implemented",  XX, XX, XX },
  /* d0 */
  //  { PREGRP27 },
  { "not implemented",  XX, XX, XX },
  //  { "psrlw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psrld",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psrlq",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddq",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pmullw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP21 },
  { "not implemented",  XX, XX, XX },
  //  { "pmovmskb",		Gdq, MS, XX },
  { "not implemented",  XX, XX, XX },
  /* d8 */
  //  { "psubusb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psubusw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pminub",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pand",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddusb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddusw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pmaxub",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pandn",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  /* e0 */
  //  { "pavgb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psraw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psrad",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pavgw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pmulhuw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pmulhw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP15 },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP25 },
  { "not implemented",  XX, XX, XX },
  /* e8 */
  //  { "psubsb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psubsw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pminsw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "por",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddsb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddsw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pmaxsw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pxor",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  /* f0 */
  //  { PREGRP32 },
  { "not implemented",  XX, XX, XX },
  //  { "psllw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pslld",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psllq",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pmuludq",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "pmaddwd",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psadbw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { PREGRP18 },
  { "not implemented",  XX, XX, XX },
  /* f8 */
  //  { "psubb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psubw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psubd",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "psubq",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddb",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddw",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  //  { "paddd",		MX, EM, XX },
  { "not implemented",  XX, XX, XX },
  { "(bad)",		XX, XX, XX }
};

// This is the original code from Kruegel's disassembler.
// struct i386_op i386_op_2[] = {
//   /* 00 */
// /* GROUPS not implemented */
// /*   { GRP6 }, */
// /*   { GRP7 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "lar",		Gv, Ew, XX },
//   { "lsl",		Gv, Ew, XX },
//   { "(bad)",		XX, XX, XX },
//   { "syscall",		XX, XX, XX },
//   { "clts",		XX, XX, XX },
//   { "sysretP",		XX, XX, XX },
//   /* 08 */
//   { "invd",		XX, XX, XX },
//   { "wbinvd",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "ud2a",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
// /* GROUPS not implemented */
// /*   { GRPAMD }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "femms",		XX, XX, XX },
// /*   MMX not supported                */
// /*   { "",			MX, EM, OPSUF }, /\* See OP_3DNowSuffix.  *\/ */
//   { "(not implemented)", XX, XX, XX },
//   /* 10 */
// /* GROUPS not implemented */
// /*   { PREGRP8 }, */
// /*   { PREGRP9 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
// /*   MMX not supported                */
// /*   { "movlpX",		XM, EX, SIMD_Fixup, 'h' }, /\* really only 2 operands *\/ */
// /*   { "movlpX",		EX, XM, SIMD_Fixup, 'h' }, */
// /*   { "unpcklpX",		XM, EX, XX }, */
// /*   { "unpckhpX",		XM, EX, XX }, */
// /*   { "movhpX",		XM, EX, SIMD_Fixup, 'l' }, */
// /*   { "movhpX",		EX, XM, SIMD_Fixup, 'l' }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   /* 18 */
// /* GROUPS not implemented */
// /*   { GRP14 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   /* 20 */
//  /* Weird Moves are not supported */
// /*   { "movL",		Rm, Cm, XX }, */
// /*   { "movL",		Rm, Dm, XX }, */
// /*   { "movL",		Cm, Rm, XX }, */
// /*   { "movL",		Dm, Rm, XX }, */
// /*   { "movL",		Rd, Td, XX }, */
//   { "(special reg mov)",XX, XX, XX },
//   { "(special reg mov)",XX, XX, XX },
//   { "(special reg mov)",XX, XX, XX },
//   { "(special reg mov)",XX, XX, XX },
//   { "(special reg mov)",XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
// /*   { "movL",		Td, Rd, XX }, */
//   { "(special reg mov)",XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   /* 28 */
// /*   MMX not supported                */
// /*   { "movapX",		XM, EX, XX }, */
// /*   { "movapX",		EX, XM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
// /* GROUPS not implemented */
// /*   { PREGRP2 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
// /*   MMX not supported                */
// /*   { "movntpX",		Ev, XM, XX }, */
//   { "(not implemented)",XX, XX, XX },   
// /* GROUPS not implemented */
// /*   { PREGRP4 }, */
// /*   { PREGRP3 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
// /*   MMX not supported                */
// /*   { "ucomisX",		XM,EX, XX }, */
// /*   { "comisX",		XM,EX, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   /* 30 */
//   { "wrmsr",		XX, XX, XX },
//   { "rdtsc",		XX, XX, XX },
//   { "rdmsr",		XX, XX, XX },
//   { "rdpmc",		XX, XX, XX },
//   { "sysenter",		XX, XX, XX },
//   { "sysexit",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   /* 38 */
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   /* 40 */
//   { "cmovo",		Gv, Ev, XX },
//   { "cmovno",		Gv, Ev, XX },
//   { "cmovb",		Gv, Ev, XX },
//   { "cmovae",		Gv, Ev, XX },
//   { "cmove",		Gv, Ev, XX },
//   { "cmovne",		Gv, Ev, XX },
//   { "cmovbe",		Gv, Ev, XX },
//   { "cmova",		Gv, Ev, XX },
//   /* 48 */
//   { "cmovs",		Gv, Ev, XX },
//   { "cmovns",		Gv, Ev, XX },
//   { "cmovp",		Gv, Ev, XX },
//   { "cmovnp",		Gv, Ev, XX },
//   { "cmovl",		Gv, Ev, XX },
//   { "cmovge",		Gv, Ev, XX },
//   { "cmovle",		Gv, Ev, XX },
//   { "cmovg",		Gv, Ev, XX },
//   /* 50 */
// /*   MMX not supported                */
// /*   { "movmskpX",		Gd, XS, XX }, */
//   { "(not implemented)", XX, XX, XX },
// /* GROUPS not implemented */
// /*   { PREGRP13 }, */
// /*   { PREGRP12 }, */
// /*   { PREGRP11 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
// /*   MMX not supported                */
// /*   { "andpX",		XM, EX, XX }, */
// /*   { "andnpX",	XM, EX, XX }, */
// /*   { "orpX",		XM, EX, XX }, */
// /*   { "xorpX",		XM, EX, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   /* 58 */
// /* GROUPS not implemented */
// /*   { PREGRP0 }, */
// /*   { PREGRP10 }, */
// /*   { PREGRP17 }, */
// /*   { PREGRP16 }, */
// /*   { PREGRP14 }, */
// /*   { PREGRP7 }, */
// /*   { PREGRP5 }, */
// /*   { PREGRP6 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   /* 60 */
// /*   MMX not supported                */
// /*   { "punpcklbw",	MX, EM, XX }, */
// /*   { "punpcklwd",	MX, EM, XX }, */
// /*   { "punpckldq",	MX, EM, XX }, */
// /*   { "packsswb",		MX, EM, XX }, */
// /*   { "pcmpgtb",		MX, EM, XX }, */
// /*   { "pcmpgtw",		MX, EM, XX }, */
// /*   { "pcmpgtd",		MX, EM, XX }, */
// /*   { "packuswb",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX }, 
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   /* 68 */
// /*   MMX not supported                */
// /*   { "punpckhbw",	MX, EM, XX }, */
// /*   { "punpckhwd",	MX, EM, XX }, */
// /*   { "punpckhdq",	MX, EM, XX }, */
// /*   { "packssdw",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
// /* GROUPS not implemented */
// /*   { PREGRP26 }, */
// /*   { PREGRP24 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* 64-bit move */
// /* GROUPS not implemented */
// /*   { PREGRP19 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   /* 70 */
// /* GROUPS not implemented */
// /*   { PREGRP22 }, */
// /*   { GRP10 }, */
// /*   { GRP11 }, */
// /*   { GRP12 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
// /*   MMX not supported                */
// /*   { "pcmpeqb",		MX, EM, XX }, */
// /*   { "pcmpeqw",		MX, EM, XX }, */
// /*   { "pcmpeqd",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "emms",		XX, XX, XX },
//   /* 78 */
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
// /* GROUPS not implemented */
// /*   { PREGRP23 }, */
// /*   { PREGRP20 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   /* 80 */
//   { "joH",		Jv, XX, XX },
//   { "jnoH",		Jv, XX, XX },
//   { "jbH",		Jv, XX, XX },
//   { "jaeH",		Jv, XX, XX },
//   { "jeH",		Jv, XX, XX },
//   { "jneH",		Jv, XX, XX },
//   { "jbeH",		Jv, XX, XX },
//   { "jaH",		Jv, XX, XX },
//   /* 88 */
//   { "jsH",		Jv, XX, XX },
//   { "jnsH",		Jv, XX, XX },
//   { "jpH",		Jv, XX, XX },
//   { "jnpH",		Jv, XX, XX },
//   { "jlH",		Jv, XX, XX },
//   { "jgeH",		Jv, XX, XX },
//   { "jleH",		Jv, XX, XX },
//   { "jgH",		Jv, XX, XX },
//   /* 90 */
//   { "seto",		Eb, XX, XX },
//   { "setno",		Eb, XX, XX },
//   { "setb",		Eb, XX, XX },
//   { "setae",		Eb, XX, XX },
//   { "sete",		Eb, XX, XX },
//   { "setne",		Eb, XX, XX },
//   { "setbe",		Eb, XX, XX },
//   { "seta",		Eb, XX, XX },
//   /* 98 */
//   { "sets",		Eb, XX, XX },
//   { "setns",		Eb, XX, XX },
//   { "setp",		Eb, XX, XX },
//   { "setnp",		Eb, XX, XX },
//   { "setl",		Eb, XX, XX },
//   { "setge",		Eb, XX, XX },
//   { "setle",		Eb, XX, XX },
//   { "setg",		Eb, XX, XX },
//   /* a0 */
//   { "pushT",		FS, XX, XX },
//   { "popT",		FS, XX, XX },
//   { "cpuid",		XX, XX, XX },
//   { "bt",		Ev, Gv, XX },
//   { "shld",		Ev, Gv, Ib },
//   { "shld",		Ev, Gv, CL },
//   { "(bad)",		XX, XX, XX },
//   { "(bad)",		XX, XX, XX },
//   /* a8 */
//   { "pushT",		GS, XX, XX },
//   { "popT",		GS, XX, XX },
//   { "rsm",		XX, XX, XX },
//   { "bts",		Ev, Gv, XX },
//   { "shrd",		Ev, Gv, Ib },
//   { "shrd",		Ev, Gv, CL },
// /* GROUPS not implemented */
// /*   { GRP13 }, */
//   { "stmxcsr (not implemented)",Ev, XX, XX },   /* Groups */
//   { "imul",		Gv, Ev, XX },
//   /* b0 */
//   { "cmpxchg",		Eb, Gb, XX },
//   { "cmpxchg",		Ev, Gv, XX },
//   { "lss",		Gv, Mp, XX },
//   { "btrS",		Ev, Gv, XX },
//   { "lfs",		Gv, Mp, XX },
//   { "lgs",		Gv, Mp, XX },
//   { "movzbR",	        Gv, Eb, XX },
//   { "movzwR",    	Gv, Ew, XX }, /* yes, there really is movzww ! */
//   /* b8 */
//   { "(bad)",		XX, XX, XX },
//   { "ud2b",		XX, XX, XX },
// /* GROUPS not implemented */
// /*   { GRP8 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "btc",		Ev, Gv, XX },
//   { "bsf",		Gv, Ev, XX },
//   { "bsr",		Gv, Ev, XX },
//   { "movsbR",   	Gv, Eb, XX },
//   { "movswR",   	Gv, Ew, XX }, /* yes, there really is movsww ! */
//   /* c0 */
//   { "xadd",		Eb, Gb, XX },
//   { "xadd",		Ev, Gv, XX },
// /* GROUPS not implemented */
// /*   { PREGRP1 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "movnti",		Ev, Gv, XX },
// /*   MMX not supported                */
// /*   { "pinsrw",		MX, Ed, Ib }, */
// /*   { "pextrw",		Gd, MS, Ib }, */
// /*   { "shufpX",		XM, EX, Ib }, */
//   { "(not implemented)",XX, XX, XX },   
//   { "(not implemented)",XX, XX, XX },   
//   { "(not implemented)",XX, XX, XX },   
// /* GROUPS not implemented */
// /*   { GRP9 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   /* c8 */
//   { "bswap",		eAX, XX, XX },
//   { "bswap",		eCX, XX, XX },
//   { "bswap",		eDX, XX, XX },
//   { "bswap",		eBX, XX, XX },
//   { "bswap",		eSP, XX, XX },
//   { "bswap",		eBP, XX, XX },
//   { "bswap",		eSI, XX, XX },
//   { "bswap",		eDI, XX, XX },
//   /* d0 */
//   { "(bad)",		XX, XX, XX },
// /*   MMX not supported                */
// /*   { "psrlw",		MX, EM, XX }, */
// /*   { "psrld",		MX, EM, XX }, */
// /*   { "psrlq",		MX, EM, XX }, */
// /*   { "paddq",		MX, EM, XX }, */
// /*   { "pmullw",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
// /* GROUPS not implemented */
// /*   { PREGRP21 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
// /*   MMX not supported                */
// /*   { "pmovmskb",		Gd, MS, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   /* d8 */
// /*   MMX not supported                */
// /*   { "psubusb",		MX, EM, XX }, */
// /*   { "psubusw",		MX, EM, XX }, */
// /*   { "pminub",		MX, EM, XX }, */
// /*   { "pand",		MX, EM, XX }, */
// /*   { "paddusb",		MX, EM, XX }, */
// /*   { "paddusw",		MX, EM, XX }, */
// /*   { "pmaxub",		MX, EM, XX }, */
// /*   { "pandn",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   /* e0 */
// /*   MMX not supported                */
// /*   { "pavgb",		MX, EM, XX }, */
// /*   { "psraw",		MX, EM, XX }, */
// /*   { "psrad",		MX, EM, XX }, */
// /*   { "pavgw",		MX, EM, XX }, */
// /*   { "pmulhuw",		MX, EM, XX }, */
// /*   { "pmulhw",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
// /* GROUPS not implemented */
// /*   { PREGRP15 }, */
// /*   { PREGRP25 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   /* e8 */
// /*   MMX not supported                */
// /*   { "psubsb",		MX, EM, XX }, */
// /*   { "psubsw",		MX, EM, XX }, */
// /*   { "pminsw",		MX, EM, XX }, */
// /*   { "por",		MX, EM, XX }, */
// /*   { "paddsb",		MX, EM, XX }, */
// /*   { "paddsw",		MX, EM, XX }, */
// /*   { "pmaxsw",		MX, EM, XX }, */
// /*   { "pxor",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   /* f0 */
//   { "(bad)",		XX, XX, XX },
// /*   MMX not supported                */
// /*   { "psllw",		MX, EM, XX }, */
// /*   { "pslld",		MX, EM, XX }, */
// /*   { "psllq",		MX, EM, XX }, */
// /*   { "pmuludq",		MX, EM, XX }, */
// /*   { "pmaddwd",		MX, EM, XX }, */
// /*   { "psadbw",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
// /* GROUPS not implemented */
// /*   { PREGRP18 }, */
//   { "(not implemented)",XX, XX, XX },   /* Groups */
//   /* f8 */
// /*   MMX not supported                */
// /*   { "psubb",		MX, EM, XX }, */
// /*   { "psubw",		MX, EM, XX }, */
// /*   { "psubd",		MX, EM, XX }, */
// /*   { "psubq",		MX, EM, XX }, */
// /*   { "paddb",		MX, EM, XX }, */
// /*   { "paddw",		MX, EM, XX }, */
// /*   { "paddd",		MX, EM, XX }, */
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(not implemented)", XX, XX, XX },
//   { "(bad)",		XX, XX, XX }
// };

const char *float_mem[] = {
  /* d8 */
  "fadds",
  "fmuls",
  "fcoms",
  "fcomps",
  "fsubs",
  "fsubrs",
  "fdivs",
  "fdivrs",
  /* d9 */
  "flds",
  "(bad)",
  "fsts",
  "fstps",
  "fldenv",
  "fldcw",
  "fNstenv",
  "fNstcw",
  /* da */
  "fiaddl",
  "fimull",
  "ficoml",
  "ficompl",
  "fisubl",
  "fisubrl",
  "fidivl",
  "fidivrl",
  /* db */
  "fildl",
  "(bad)",
  "fistl",
  "fistpl",
  "(bad)",
  "fldt",
  "(bad)",
  "fstpt",
  /* dc */
  "faddl",
  "fmull",
  "fcoml",
  "fcompl",
  "fsubl",
  "fsubrl",
  "fdivl",
  "fdivrl",
  /* dd */
  "fldl",
  "(bad)",
  "fstl",
  "fstpl",
  "frstor",
  "(bad)",
  "fNsave",
  "fNstsw",
  /* de */
  "fiadd",
  "fimul",
  "ficom",
  "ficomp",
  "fisub",
  "fisubr",
  "fidiv",
  "fidivr",
  /* df */
  "fild",
  "(bad)",
  "fist",
  "fistp",
  "fbld",
  "fildll",
  "fbstp",
  "fistpll",
};

struct i386_op float_reg[][8] = {
  /* d8 */
  {
    { "fadd",	ST, STi, XX },
    { "fmul",	ST, STi, XX },
    { "fcom",	STi, XX, XX },
    { "fcomp",	STi, XX, XX },
    { "fsub",	ST, STi, XX },
    { "fsubr",	ST, STi, XX },
    { "fdiv",	ST, STi, XX },
    { "fdivr",	ST, STi, XX },
  },
  /* d9 */
  {
    { "fld",	STi, XX, XX },
    { "fxch",	STi, XX, XX },
    { FGRPd9_2 },
    { "(bad)",	XX, XX, XX },
    { FGRPd9_4 },
    { FGRPd9_5 },
    { FGRPd9_6 },
    { FGRPd9_7 },
  },
  /* da */
  {
    { "fcmovb",	ST, STi, XX },
    { "fcmove",	ST, STi, XX },
    { "fcmovbe",ST, STi, XX },
    { "fcmovu",	ST, STi, XX },
    { "(bad)",	XX, XX, XX },
    { FGRPda_5 },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
  },
  /* db */
  {
    { "fcmovnb",ST, STi, XX },
    { "fcmovne",ST, STi, XX },
    { "fcmovnbe",ST, STi, XX },
    { "fcmovnu",ST, STi, XX },
    { FGRPdb_4 },
    { "fucomi",	ST, STi, XX },
    { "fcomi",	ST, STi, XX },
    { "(bad)",	XX, XX, XX },
  },
  /* dc */
  {
    { "fadd",	STi, ST, XX },
    { "fmul",	STi, ST, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "fsub",	STi, ST, XX },
    { "fsubr",	STi, ST, XX },
    { "fdiv",	STi, ST, XX },
    { "fdivr",	STi, ST, XX },
  },
  /* dd */
  {
    { "ffree",	STi, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "fst",	STi, XX, XX },
    { "fstp",	STi, XX, XX },
    { "fucom",	STi, XX, XX },
    { "fucomp",	STi, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
  },
  /* de */
  {
    { "faddp",	STi, ST, XX },
    { "fmulp",	STi, ST, XX },
    { "(bad)",	XX, XX, XX },
    { FGRPde_3 },
    { "fsubp",	STi, ST, XX },
    { "fsubrp",	STi, ST, XX },
    { "fdivp",	STi, ST, XX },
    { "fdivrp",	STi, ST, XX },
  },
  /* df */
  {
    { "ffreep",	STi, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
    { FGRPdf_4 },
    { "fucomip",ST, STi, XX },
    { "fcomip", ST, STi, XX },
    { "(bad)",	XX, XX, XX },
  },
};

const char *fgrps[][8] = {
  /* d9_2  0 */
  {
    "fnop","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* d9_4  1 */
  {
    "fchs","fabs","(bad)","(bad)","ftst","fxam","(bad)","(bad)",
  },

  /* d9_5  2 */
  {
    "fld1","fldl2t","fldl2e","fldpi","fldlg2","fldln2","fldz","(bad)",
  },

  /* d9_6  3 */
  {
    "f2xm1","fyl2x","fptan","fpatan","fxtract","fprem1","fdecstp","fincstp",
  },

  /* d9_7  4 */
  {
    "fprem","fyl2xp1","fsqrt","fsincos","frndint","fscale","fsin","fcos",
  },

  /* da_5  5 */
  {
    "(bad)","fucompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* db_4  6 */
  {
    "feni(287 only)","fdisi(287 only)","fNclex","fNinit",
    "fNsetpm(287 only)","(bad)","(bad)","(bad)",
  },

  /* de_3  7 */
  {
    "(bad)","fcompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* df_4  8 */
  {
    "fNstsw","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },
};


struct i386_op i386_group_1[][8] = {
  /* GRP0 */
  {
    { "addA",	Eb, Ib, XX },
    { "orA",	Eb, Ib, XX },
    { "adcA",	Eb, Ib, XX },
    { "sbbA",	Eb, Ib, XX },
    { "andA",	Eb, Ib, XX },
    { "subA",	Eb, Ib, XX },
    { "xorA",	Eb, Ib, XX },
    { "cmpA",	Eb, Ib, XX }
  },
  /* GRP1 */
  {
    { "addQ",	Ev, Iv, XX },
    { "orQ",	Ev, Iv, XX },
    { "adcQ",	Ev, Iv, XX },
    { "sbbQ",	Ev, Iv, XX },
    { "andQ",	Ev, Iv, XX },
    { "subQ",	Ev, Iv, XX },
    { "xorQ",	Ev, Iv, XX },
    { "cmpQ",	Ev, Iv, XX }
  },
  /* GRP2 */
  {
    { "add",	Eb, sIb, XX },
    { "or",	Eb, sIb, XX },
    { "adc",	Eb, sIb, XX },
    { "sbb",	Eb, sIb, XX },
    { "and",	Eb, sIb, XX },
    { "sub",	Eb, sIb, XX },
    { "xor",	Eb, sIb, XX },
    { "cmp",	Eb, sIb, XX }
  },
  /* GRP3 */
  {
    { "addQ",	Ev, sIb, XX },
    { "orQ",	Ev, sIb, XX },
    { "adcQ",	Ev, sIb, XX },
    { "sbbQ",	Ev, sIb, XX },
    { "andQ",	Ev, sIb, XX },
    { "subQ",	Ev, sIb, XX },
    { "xorQ",	Ev, sIb, XX },
    { "cmpQ",	Ev, sIb, XX }
  },
  /* GRP4 */
  {
    { "rolA",	Eb, Ib, XX },
    { "rorA",	Eb, Ib, XX },
    { "rclA",	Eb, Ib, XX },
    { "rcrA",	Eb, Ib, XX },
    { "shlA",	Eb, Ib, XX },
    { "shrA",	Eb, Ib, XX },
    { "(bad)",	XX, XX, XX },
    { "sarA",	Eb, Ib, XX },
  },
  /* GRP5 */
  {
    { "rolQ",	Ev, Ib, XX },
    { "rorQ",	Ev, Ib, XX },
    { "rclQ",	Ev, Ib, XX },
    { "rcrQ",	Ev, Ib, XX },
    { "shlQ",	Ev, Ib, XX },
    { "shrQ",	Ev, Ib, XX },
    { "(bad)",	XX, XX, XX },
    { "sarQ",	Ev, Ib, XX },
  },
  /* GRP6 */
  {
    { "rolA",	Eb, XX, XX },
    { "rorA",	Eb, XX, XX },
    { "rclA",	Eb, XX, XX },
    { "rcrA",	Eb, XX, XX },
    { "shlA",	Eb, XX, XX },
    { "shrA",	Eb, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "sarA",	Eb, XX, XX },
  },
  /* GRP7 */
  {
    { "rolQ",	Ev, XX, XX },
    { "rorQ",	Ev, XX, XX },
    { "rclQ",	Ev, XX, XX },
    { "rcrQ",	Ev, XX, XX },
    { "shlQ",	Ev, XX, XX },
    { "shrQ",	Ev, XX, XX },
    { "(bad)",	XX, XX, XX},
    { "sarQ",	Ev, XX, XX },
  },
  /* GRP8 */
  {
    { "rolA",	Eb, CL, XX },
    { "rorA",	Eb, CL, XX },
    { "rclA",	Eb, CL, XX },
    { "rcrA",	Eb, CL, XX },
    { "shlA",	Eb, CL, XX },
    { "shrA",	Eb, CL, XX },
    { "(bad)",	XX, XX, XX },
    { "sarA",	Eb, CL, XX },
  },
  /* GRP9 */
  {
    { "rolQ",	Ev, CL, XX },
    { "rorQ",	Ev, CL, XX },
    { "rclQ",	Ev, CL, XX },
    { "rcrQ",	Ev, CL, XX },
    { "shlQ",	Ev, CL, XX },
    { "shrQ",	Ev, CL, XX },
    { "(bad)",	XX, XX, XX },
    { "sarQ",	Ev, CL, XX }
  },
  /* GRP10 */
  {
    { "testA",	Eb, Ib, XX },
    { "(bad)",	Eb, XX, XX },
    { "notA",	Eb, XX, XX },
    { "negA",	Eb, XX, XX },
    { "mulA",	Eb, XX, XX },
    { "imulA",	Eb, XX, XX },
    { "divA",	Eb, XX, XX },
    { "idivA",	Eb, XX, XX }	
  },
  /* GRP11 */
  {
    { "testQ",	Ev, Iv, XX },
    { "(bad)",	XX, XX, XX },
    { "notQ",	Ev, XX, XX },
    { "negQ",	Ev, XX, XX },
    { "mulQ",	Ev, XX, XX },	
    { "imulQ",	Ev, XX, XX },
    { "divQ",	Ev, XX, XX },
    { "idivQ",	Ev, XX, XX },
  },
  /* GRP12 */
  {
    { "incA",	Eb, XX, XX },
    { "decA",	Eb, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
    { "(bad)",	XX, XX, XX },
  },
  /* GRP13 */
  {
    { "incQ",	Ev, XX, XX },
    { "decQ",	Ev, XX, XX },
    { "callP",	indirEv, XX, XX },
    { "lcallP",	indirEv, XX, XX },
    { "jmpP",	indirEv, XX, XX },
    { "ljmpP",	indirEv, XX, XX },
    { "pushQ",	Ev, XX, XX },
    { "(bad)",	XX, XX, XX },
  }
};

struct i386_op i386_group_2[][8] = {
  {}
};
