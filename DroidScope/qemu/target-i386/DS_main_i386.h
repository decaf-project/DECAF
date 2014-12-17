/**
 * Copyright (C) <2013> <Syracuse System Security (Sycure) Lab>
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
**/

/**
 * @author Lok Yan
 * @date 1 JAN 2013
**/

#ifndef DS_MAIN_I386_H
#define DS_MAIN_I386_H

#include "DECAF_shared/DECAF_types.h"

/*copied over from header.S
The following registers have fixed assignments:

  reg nick      purpose
  r4  rPC       interpreted program counter, used for fetching instructions
  r5  rFP       interpreted frame pointer, used for accessing locals and args
  r6  rGLUE     MterpGlue pointer
  r7  rINST     first 16-bit code unit of current instruction
  r8  rIBASE    interpreted instruction base pointer, used for computed goto
*/

#define rPC R_EDX 
#define rFP R_ESI
#define rINST R_EBX
#define rIBASE R_EDI


#endif
