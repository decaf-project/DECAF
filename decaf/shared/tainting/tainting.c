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
#include "shared/tainting/tainting.h"
#include "shared/tainting/taint_memory.h"

void tainting_init(void)
{
#ifdef CONFIG_TCG_TAINT
        /* AWH - Taint tracking IR setup (testing) */
        do_enable_tainting_internal();
#endif /* CONFIG_TCG_TAINT */
}

void tainting_cleanup(void)
{
  //nothing to do
}

