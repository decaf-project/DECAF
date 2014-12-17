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
#ifndef _DISASM_PP_H
#define _DISASM_PP_H

// #include "disasm.h"
#include <stdint.h>
#include <libiberty.h>
//#include <irtoir.h>

#ifdef __cplusplus

#include <iostream>
#include <sstream>

void ostream_i386_register(int regnum, std::ostream &out);

void
ostream_i386_mnemonic(Instruction *inst, std::ostream &out);

void ostream_i386_insn(Instruction *inst, std::ostream &out);

extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
