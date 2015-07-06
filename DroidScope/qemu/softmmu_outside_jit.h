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
#ifndef QEMU_SOFTMMU_OUTSIDE_JIT_H
#define QEMU_SOFTMMU_OUTSIDE_JIT_H

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Declares routines implemented in softmmu_outside_jit.c, that are used in
// this macros expansion. Note that MMUSUFFIX _outside_jit is enforced in
// softmmu_header.h by defining OUTSIDE_JIT macro.
////////////////////////////////////////////////////////////////////////////////

uint8_t REGPARM __ldb_outside_jit(target_ulong addr, int mmu_idx);
void REGPARM __stb_outside_jit(target_ulong addr, uint8_t val, int mmu_idx);
uint16_t REGPARM __ldw_outside_jit(target_ulong addr, int mmu_idx);
void REGPARM __stw_outside_jit(target_ulong addr, uint16_t val, int mmu_idx);
uint32_t REGPARM __ldl_outside_jit(target_ulong addr, int mmu_idx);
void REGPARM __stl_outside_jit(target_ulong addr, uint32_t val, int mmu_idx);
uint64_t REGPARM __ldq_outside_jit(target_ulong addr, int mmu_idx);
void REGPARM __stq_outside_jit(target_ulong addr, uint64_t val, int mmu_idx);

// Enforces MMUSUFFIX to be set to _outside_jit in softmmu_header.h
#define OUTSIDE_JIT
// Enforces use of cpu_single_env for CPU environment.
#define env cpu_single_env

// =============================================================================
// Generate ld/stx_user
// =============================================================================
#if defined(TARGET_MIPS)
#define MEMSUFFIX MMU_MODE2_SUFFIX
#else
#define MEMSUFFIX MMU_MODE1_SUFFIX
#endif
#define ACCESS_TYPE 1

#define DATA_SIZE 1
#include "softmmu_header.h"

#define DATA_SIZE 2
#include "softmmu_header.h"

#define DATA_SIZE 4
#include "softmmu_header.h"

#define DATA_SIZE 8
#include "softmmu_header.h"

#undef MEMSUFFIX
#undef ACCESS_TYPE

// =============================================================================
// Generate ld/stx_kernel
// =============================================================================
#define MEMSUFFIX MMU_MODE0_SUFFIX
#define ACCESS_TYPE 0

#define DATA_SIZE 1
#include "softmmu_header.h"

#define DATA_SIZE 2
#include "softmmu_header.h"

#define DATA_SIZE 4
#include "softmmu_header.h"

#define DATA_SIZE 8
#include "softmmu_header.h"

#undef MEMSUFFIX
#undef ACCESS_TYPE

#undef env
#undef OUTSIDE_JIT

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif  // QEMU_SOFTMMU_OUTSIDE_JIT_H
