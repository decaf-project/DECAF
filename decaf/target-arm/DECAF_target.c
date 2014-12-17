/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU LGPL, version 2.1 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
#include <assert.h>
#include <sys/queue.h>
#include "sysemu.h" // AWH
#include "qemu-timer.h" // AWH
#include "hw/hw.h"
#include "hw/isa.h"		/* for register_ioport_write */
#include "blockdev.h" // AWH
#include "shared/DECAF_main.h" // AWH
#include "shared/DECAF_callback.h"
#include "shared/hookapi.h" // AWH
#include "DECAF_target.h"


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



