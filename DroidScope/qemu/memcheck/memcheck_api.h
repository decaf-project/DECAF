/* Copyright (C) 2007-2010 The Android Open Source Project
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
 * Contains declarations of memchecker external variables and routines, used by
 * other qemu components.
 */

#ifndef QEMU_MEMCHECK_MEMCHECK_API_H
#define QEMU_MEMCHECK_MEMCHECK_API_H

/* This file should compile iff qemu is built with memory checking
 * configuration turned on. */
#ifndef CONFIG_MEMCHECK
#error CONFIG_MEMCHECK is not defined.
#endif  // CONFIG_MEMCHECK

/* Global flag, indicating whether or not memchecking has been enabled
 * for the current emulator session. 1 means that memchecking has been
 * enabled, 0 means that memchecking has not been enabled. The variable
 * is declared in memchec/memcheck.c */
extern int memcheck_enabled;

/* Flags wether or not mmu instrumentation is enabled by memchecker.
 * 1 - enabled, 0 - is not enabled. */
extern int memcheck_instrument_mmu;

/* Global flag, indicating whether or not memchecker is collecting call stack.
 * 1 - call stack is being collected, 0 means that stack is not being
 * collected. The variable is declared in memchec/memcheck.c */
extern int memcheck_watch_call_stack;

/* Array of (tb_pc, guest_pc) pairs, big enough for all translations. This
 * array is used to obtain guest PC address from a translated PC address.
 * tcg_gen_code_common will fill it up when memchecker is enabled. The array is
 * declared in ./translate_all.c */
extern void** gen_opc_tpc2gpc_ptr;

/* Number of (tb_pc, guest_pc) pairs stored in gen_opc_tpc2gpc array.
 * The variable is declared in ./translate_all.c */
extern unsigned int gen_opc_tpc2gpc_pairs;

/* Checks if given address range in the context of the current process is
 * under surveillance by memchecker.
 * Param:
 *  addr - Starting address of a range.
 *  size - Range size.
 * Return:
 *  boolean: 1 if address range contains memory that requires access
 *  violation detection, or 0 if given address range is in no interest to
 *  the memchecker. */
int memcheck_is_checked(target_ulong addr, uint32_t size);

/* Validates __ldx_mmu operations.
 * Param:
 *  addr - Virtual address in the guest space where memory is read.
 *  data_size - Size of the read.
 *  retaddr - Code address (in TB) that accesses memory.
 * Return:
 *  1 Address should be invalidated in TLB cache, in order to ensure that
 *  subsequent attempts to read from that page will launch __ld/__stx_mmu.
 *  If this routine returns zero, no page invalidation is requried.
 */
int memcheck_validate_ld(target_ulong addr,
                         uint32_t data_size,
                         target_ulong retaddr);

/* Validates __stx_mmu operations.
 * Param:
 *  addr - Virtual address in the guest space where memory is written.
 *  data_size - Size of the write.
 *  value - Value to be written. Note that we typecast all values to 64 bits,
 *      since this will fit all data sizes.
 *  retaddr - Code address (in TB) that accesses memory.
 * Return:
 *  1 Address should be invalidated in TLB cache, in order to ensure that
 *  subsequent attempts to read from that page will launch __ld/__stx_mmu.
 *  If this routine returns zero, no page invalidation is requried.
 */
int memcheck_validate_st(target_ulong addr,
                         uint32_t data_size,
                         uint64_t value,
                         target_ulong retaddr);

/* Memchecker's handler for on_call callback.
 * Param:
 *  pc - Guest address where call has been made.
 *  ret - Guest address where called routine will return.
 */
void memcheck_on_call(target_ulong pc, target_ulong ret);

/* Memchecker's handler for on_ret callback.
 * Param:
 *  pc - Guest address where routine has returned.
 */
void memcheck_on_ret(target_ulong pc);

#endif  // QEMU_MEMCHECK_MEMCHECK_API_H
