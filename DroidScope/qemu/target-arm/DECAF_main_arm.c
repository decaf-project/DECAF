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

#include "DECAF_shared/DECAF_main.h"

gva_t DECAF_getPC(CPUState* env)
{
  if (env == NULL)
  {
    return (INV_ADDR);
  }
  return (env->regs[15]);
}

//Based this off of helper.c in get_level1_table_address
gpa_t DECAF_getPGD(CPUState* env)
{
  if (env == NULL)
  {
    return (INV_ADDR);
  }

  return (env->cp15.c2_base0 & env->cp15.c2_base_mask);
}
 
gva_t DECAF_getESP(CPUState* env)
{
  if (env == NULL)
  {
    return (INV_ADDR);
  }
  return (env->regs[13]);
}

target_ulong DECAF_getFirstParam(CPUState* env)
{
  if (env == NULL)
  {
    //what to do?
    return (0);
  }
  return (env->regs[0]);
}

gva_t DECAF_getReturnAddr(CPUState* env)
{
  if (env == NULL)
  {
    return (INV_ADDR);
  }
  return (env->regs[14]);
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


  gpa_t old = env->cp15.c2_base0;
  gpa_t old1 = env->cp15.c2_base1;
  gpa_t phys_addr;

  env->cp15.c2_base0 = pgd;
  env->cp15.c2_base1 = pgd;

  phys_addr = cpu_get_phys_page_debug(env, addr & TARGET_PAGE_MASK);

  env->cp15.c2_base0 = old;
  env->cp15.c2_base1 = old1;

  return (phys_addr | (addr & (~TARGET_PAGE_MASK)));
}

