/* Copyright (C) 2007-2008 The Android Open Source Project
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
#ifndef GOLDFISH_VMEM_H
#define GOLDFISH_VMEM_H

// Call these functions instead of cpu_memory_rw_debug and
// cpu_get_phys_page_debug to ensure virtual address translation always works
// properly, and efficently, under KVM.

int safe_memory_rw_debug(CPUState *env, target_ulong addr, uint8_t *buf,
                         int len, int is_write);

target_phys_addr_t safe_get_phys_page_debug(CPUState *env, target_ulong addr);


#endif  /* GOLDFISH_VMEM_H */
