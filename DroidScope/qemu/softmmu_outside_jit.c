/* Copyright (C) 2007-2009 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/*
 * Contains SOFTMMU macros expansion for ldx_user and stx_user routines used
 * outside of JIT. The issue is that regular implementation of these routines
 * assumes that pointer to CPU environment is stored in ebp register, which
 * is true for calls made inside JIT, but is not necessarily true for calls
 * made outside of JIT. The way SOFTMMU macros are expanded in this header
 * enforces ldx/stx routines to use CPU environment stored in cpu_single_env
 * variable.
 */

#include "qemu-common.h"
#include "cpu.h"
#include "exec-all.h"

#define OUTSIDE_JIT
#define MMUSUFFIX       _outside_jit
#define GETPC()         NULL
#define env             cpu_single_env
#define ACCESS_TYPE     1
#define CPU_MMU_INDEX   (cpu_mmu_index(env))

#define SHIFT 0
#include "softmmu_template.h"

#define SHIFT 1
#include "softmmu_template.h"

#define SHIFT 2
#include "softmmu_template.h"

#define SHIFT 3
#include "softmmu_template.h"

#undef CPU_MMU_INDEX
#undef ACCESS_TYPE
#undef env
#undef MMUSUFFIX
