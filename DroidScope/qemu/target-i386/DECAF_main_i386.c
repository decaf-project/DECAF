/**
 * Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>
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
 * @date 13 DEC 2012
**/

#include "DECAF_shared/DECAF_main.h"

gva_t DECAF_getPC(CPUState* env)
{
  if (env == NULL)
  {
    return (INV_ADDR);
  }

  return (env->eip + env->segs[R_CS].base);
}

gpa_t DECAF_getPGD(CPUState* env)
{
  if (env == NULL)
  {
    return (INV_ADDR);
  } 
  
  return (env->cr[3]);
}

gva_t DECAF_getESP(CPUState* env)
{
  if (env == NULL)
  {
    return (INV_ADDR);
  }

  return (env->regs[R_ESP]);
}

target_ulong DECAF_getFirstParam(CPUState* env)
{
  if (env == NULL)
  {
    return (0);
  }
  return (env->regs[R_EAX]);
}

gva_t DECAF_getReturnAddr(CPUState* env)
{
  gva_t temp;

  if (env == NULL)
  {
    return (INV_ADDR);
  }

  DECAF_read_mem(env, env->regs[R_ESP], &temp, sizeof(temp));
  return (temp);
}

gpa_t DECAF_get_phys_addr_with_pgd(CPUState* env, gpa_t pgd, gva_t addr)
{

  if (env == NULL)
  {
#ifdef DECAF_NO_FAIL_SAFE
    return (INV_ADDR);
#else
    env = cpu_single_env ? cpu_single_env : first_cpu;
#endif
  }

  target_ulong saved_cr3 = env->cr[3];
  uint32_t phys_addr;

  env->cr[3] = pgd;
  phys_addr = cpu_get_phys_page_debug(env, addr & TARGET_PAGE_MASK);

  env->cr[3] = saved_cr3;
  return (phys_addr | (addr & (~TARGET_PAGE_MASK)));
}

