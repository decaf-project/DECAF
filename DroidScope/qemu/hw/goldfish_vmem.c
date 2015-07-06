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
#include "hw.h"
#include "goldfish_vmem.h"
#ifdef TARGET_I386
#include "kvm.h"
#endif


// Both safe_memory_rw_debug and safe_get_phys_page_debug need to translate
// virtual addresses to physical addresses. When running on KVM we need to
// pull the cr registers and hflags from the VCPU. These functions wrap the
// calls to kvm_get_sregs to pull these registers over when necessary.
//
// Note: we do not call the cpu_synchronize_state function because that pulls
// all the VCPU registers. That equates to 4 ioctls on the KVM virtual device
// and on AMD some of those ioctls (in particular KVM_GET_MSRS) are 10 to 100x
// slower than on Intel chips.

int safe_memory_rw_debug(CPUState *env, target_ulong addr, uint8_t *buf,
                         int len, int is_write)
{
#ifdef TARGET_I386
    if (kvm_enabled()) {
        kvm_get_sregs(env);
    }
#endif
    return cpu_memory_rw_debug(env, addr, buf, len, is_write);
}

target_phys_addr_t safe_get_phys_page_debug(CPUState *env, target_ulong addr)
{
#ifdef TARGET_I386
    if (kvm_enabled()) {
        kvm_get_sregs(env);
    }
#endif
    return cpu_get_phys_page_debug(env, addr);
}

