/*
 * LibOpcodesWrapper_ARM.h
 *
 *  Created on: Sep 8, 2011
 *      Author: lok
 */

#ifndef LIBOPCODESWRAPPER_ARM_H_
#define LIBOPCODESWRAPPER_ARM_H_

#include <bfd.h>
#include <dis-asm.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <inttypes.h>

class LibOpcodesWrapper_ARM
{
public:
  LibOpcodesWrapper_ARM();
  int disassemble(std::string& str, uint32_t i, bool isThumb);

  friend void printfWrapper(void* result, const char* fmt, ...);
  friend void printAddressWrapper(bfd_vma addr, struct disassemble_info* pInfo);
private:
  struct disassemble_info info;
  uint32_t insn;
  std::string* pResult;
};

int disassemble_ARM(std::string& str, uint32_t i, bool isThumb);

#endif /* LIBOPCODESWRAPPER_ARM_H_ */
