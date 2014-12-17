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
/************************************************************
 * reg_ids.h
 *
 * Author: Cody Hartwig <chartwig@cs.cmu.edu>
 * 		   Heng Yin <hyin@ece.cmu.edu>
 *
 * mapping from QEMU constant to register.
 */

#ifndef _REG_IDS_H_
#define _REG_IDS_H_

#include "disasm.h"
#include <assert.h>

#define R_EAX 0
#define R_ECX 1
#define R_EDX 2
#define R_EBX 3
#define R_ESP 4
#define R_EBP 5
#define R_ESI 6
#define R_EDI 7

#define eip_reg 140
#define cr3_reg 141
#define t0_reg	142
#define t1_reg 	143
#define a0_reg	144

static inline const char *reg_name_from_id(int reg_id)
{
  switch(reg_id) {
    case es_reg: return "R_ES";
    case cs_reg: return "R_CS";
    case ss_reg: return "R_SS";
    case ds_reg: return "R_DS";
    case fs_reg: return "R_FS";
    case gs_reg: return "R_GS";


    case eax_reg: return "R_EAX";
    case ecx_reg: return "R_ECX";
    case edx_reg: return "R_EDX";
    case ebx_reg: return "R_EBX";
    case esp_reg: return "R_ESP"; 
    case ebp_reg: return "R_EBP";
    case esi_reg: return "R_ESI";
    case edi_reg: return "R_EDI";
    case eip_reg: return "R_EIP";

    case al_reg: return "R_AL";
    case cl_reg: return "R_CL";
    case dl_reg: return "R_DL";
    case bl_reg: return "R_BL";
    case ah_reg: return "R_AH"; 
    case ch_reg: return "R_CH";
    case dh_reg: return "R_DH";
    case bh_reg: return "R_BH";
    
    case ax_reg: return "R_AX";
    case cx_reg: return "R_CX";
    case dx_reg: return "R_DX";
    case bx_reg: return "R_BX";
    case sp_reg: return "R_SP"; 
    case bp_reg: return "R_BP";
    case si_reg: return "R_SI";
    case di_reg: return "R_DI";

    default: return NULL;
  }
}

static inline void reg_index_from_id(int reg_id, unsigned int *base_id, 
	unsigned int *offset, unsigned int *size)
{
  switch(reg_id) {
    case eax_reg: *base_id = R_EAX, *offset=0, *size=4;
    	break;
    case ecx_reg: *base_id = R_ECX, *offset=0, *size=4;
    	break;
    case edx_reg: *base_id = R_EDX, *offset=0, *size=4;
    	break;
    case ebx_reg: *base_id = R_EBX, *offset=0, *size=4;
    	break;
    case esp_reg: *base_id = R_ESP, *offset=0, *size=4;
    	break;
    case ebp_reg: *base_id = R_EBP, *offset=0, *size=4;
    	break;
    case esi_reg: *base_id = R_ESI, *offset=0, *size=4;
    	break;
    case edi_reg: *base_id = R_EDI, *offset=0, *size=4;
    	break;

    case al_reg: *base_id = R_EAX, *offset=0, *size=1;
			   break;
    case cl_reg: *base_id = R_ECX, *offset=0, *size=1;
			   break;
    case dl_reg: *base_id = R_EDX, *offset=0, *size=1;
			   break;
    case bl_reg: *base_id = R_EBX, *offset=0, *size=1;
			   break;
    case ah_reg: *base_id = R_EAX, *offset=1, *size=1;
			   break;
    case ch_reg: *base_id = R_ECX, *offset=1, *size=1;
			   break;
    case dh_reg: *base_id = R_EDX, *offset=1, *size=1;
			   break;
    case bh_reg: *base_id = R_EBX, *offset=1, *size=1;
			   break;
       
    case ax_reg: *base_id = R_EAX, *offset=0, *size=2; 
			   break;
    case cx_reg: *base_id = R_ECX, *offset=0, *size=2; 
			   break;
    case dx_reg: *base_id = R_EDX, *offset=0, *size=2; 
			   break;
    case bx_reg: *base_id = R_EBX, *offset=0, *size=2; 
			   break;
    case sp_reg: *base_id = R_ESP, *offset=0, *size=2; 
			   break;
    case bp_reg: *base_id = R_EBP, *offset=0, *size=2; 
			   break;
    case si_reg: *base_id = R_ESI, *offset=0, *size=2; 
			   break;
    case di_reg: *base_id = R_EDI, *offset=0, *size=2; 
			   break;

    case es_reg: *base_id = -1, *offset = 0, *size=2; break;
    case cs_reg: *base_id = -1, *offset = 0, *size=2; break;
    case ss_reg: *base_id = -1, *offset = 0, *size=2; break;
    case ds_reg: *base_id = -1, *offset = 0, *size=2; break;
    case fs_reg: *base_id = -1, *offset = 0, *size=2; break;
    case gs_reg: *base_id = -1, *offset = 0, *size = 2; break;
    default: *base_id = -1;
  }
}

static inline int reg_index_to_id(unsigned int base, unsigned int offset, 
	unsigned int size)
{
  switch (base) {
  case R_EAX: 
	{
	  switch(size) {
		case 1: if(offset == 0) return al_reg;
				else if(offset == 1) return ah_reg;
				assert(0);
				break;
		case 2: if(offset == 0) return ax_reg;
				assert(0);
				break;
		case 4: assert(offset == 0);
				return eax_reg;
		default:
				assert(0);
	  }
	  break;
	}

  case R_ECX:
 	{
	  switch(size) {
		case 1: if(offset == 0) return cl_reg;
				else if(offset == 1) return ch_reg;
				assert(0);
				break;
		case 2: if(offset == 0) return cx_reg;
				assert(0);
				break;
		case 4: assert(offset == 0);
				return ecx_reg;
		default:
				assert(0);
	  }
	  break;
	}
 
  case R_EDX:
  	{
	  switch(size) {
		case 1: if(offset == 0) return dl_reg;
				else if(offset == 1) return dh_reg;
				assert(0);
				break;
		case 2: if(offset == 0) return dx_reg;
				assert(0);
				break;
		case 4: assert(offset == 0);
				return edx_reg;
		default:
				assert(0);
	  }
	  break;
	}

  case R_EBX:
 	{
	  switch(size) {
		case 1: if(offset == 0) return bl_reg;
				else if(offset == 1) return bh_reg;
				assert(0);
				break;
		case 2: if(offset == 0) return bx_reg;
				assert(0);
				break;
		case 4: assert(offset == 0);
				return ebx_reg;
		default:
				assert(0);
	  }
	  break;
	}
 
  case R_EBP:
	{
	  assert(offset == 0);
	  if(size == 2) return bp_reg;
	  else if(size == 4) return ebp_reg;
	  assert(0);
	  break;
	}
  
  case R_ESP:
  	{
	  assert(offset == 0);
	  if(size == 2) return sp_reg;
	  else if(size == 4) return esp_reg;
	  assert(0);
	  break;
	}

  case R_EDI:
 	{
	  assert(offset == 0);
	  if(size == 2) return di_reg;
	  else if(size == 4) return edi_reg;
	  assert(0);
	  break;
	}
 
  case R_ESI:
 	{
	  assert(offset == 0);
	  if(size == 2) return si_reg;
	  else if(size == 4) return esi_reg;
	  assert(0);
	  break;
	}
 
  default: 
    assert(0);
  }
  assert(0);
  return 0;
}

#endif
