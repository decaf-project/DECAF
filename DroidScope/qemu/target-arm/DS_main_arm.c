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
 * 
 * @author Lok Yan
 * @date 1 Jan 2013
**/

#include "DS_main_arm.h"

inline target_ulong getDalvikPC(CPUState* env)
{
  return (env->regs[rPC]);
}

inline target_ulong getDalvikFP(CPUState* env)
{
  return (env->regs[rFP]);
}

inline target_ulong getDalvikGLUE(CPUState* env)
{
  return (env->regs[rGLUE]);
}

inline target_ulong getDalvikINST(CPUState* env)
{
  return (env->regs[rINST]);
}

inline target_ulong getDalvikIBASE(CPUState* env)
{
  return (env->regs[rIBASE]);
}
