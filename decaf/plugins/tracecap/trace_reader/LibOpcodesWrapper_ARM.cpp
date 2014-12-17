/*
 * LibOpcodesWrapper_ARM.cpp
 *
 *  Created on: Sep 8, 2011
 *      Author: lok
 */

#include <stdlib.h>
#include <cstdarg>
#include "LibOpcodesWrapper_ARM.h"


 void printfWrapper(void* result, const char* fmt, ...)
 {
   if (result == NULL || fmt == NULL)
   {
     return;
   }
   std::string* pStr= *(std::string**)(result);
   if (pStr == NULL)
   {
     return;
   }

   static char buffer[1024];
   va_list args;
   va_start (args, fmt);
   vsnprintf(buffer, 1023, fmt, args);
   va_end (args);

   pStr->append(buffer);
   buffer[0] = '\0';
 }

 void printAddressWrapper(bfd_vma addr, struct disassemble_info* pInfo)
 {
   //nothing to do here
 }

LibOpcodesWrapper_ARM::LibOpcodesWrapper_ARM()
{
  init_disassemble_info(&info, (void*)(&pResult), (fprintf_ftype)printfWrapper);
  info.print_address_func = printAddressWrapper;
  info.arch = bfd_arch_arm;
  info.mach = 0;
  info.buffer_vma = 0;
  info.buffer_length = 4;
  info.section = NULL;
  info.buffer = (bfd_byte*)(&insn);

  disassemble_init_for_target(&info);
}
#include <iostream>
int LibOpcodesWrapper_ARM::disassemble(std::string& str, uint32_t i, bool isThumb)
{
  static char force_thumb[] = "force-thumb";
  static char no_force_thumb[] = "no-force-thumb";
  insn = i;
  pResult = &(str);
  if (isThumb)
  {
    info.disassembler_options = force_thumb;
  }
  else
  {
    info.disassembler_options = no_force_thumb;
  }
//std::cout << isThumb << std::hex << (uint32_t)(info.buffer[0]) << " " << (uint32_t)(info.buffer[1]) << " " << std::endl;
  //print_insn_little_arm(0, &info);
  return (0);
}

int disassemble_ARM(std::string& str, uint32_t i, bool isThumb)
{
  static LibOpcodesWrapper_ARM dis;

  return (dis.disassemble(str, i, isThumb));
}
