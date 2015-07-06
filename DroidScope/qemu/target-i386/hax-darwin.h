/*
** Copyright (c) 2011, Intel Corporation
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

#ifndef __HAX_UNIX_H
#define __HAX_UNIX_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdarg.h>

#define HAX_INVALID_FD  (-1)
static inline int hax_invalid_fd(hax_fd fd)
{
    return fd <= 0;
}

static inline void hax_mod_close(struct hax_state *hax)
{
    close(hax->fd);
}

static inline void hax_close_fd(hax_fd fd)
{
    close(fd);
}

/* HAX model level ioctl */
/* Get API version the HAX driver supports */
#define HAX_IOCTL_VERSION _IOWR(0, 0x20, struct hax_module_version)
/* Create VM instance and return the vm_id */
#define HAX_IOCTL_CREATE_VM _IOWR(0, 0x21, int)
/* Get HAXM capability information */
#define HAX_IOCTL_CAPABILITY _IOR(0, 0x23, struct hax_capabilityinfo)

/* Pass down a VM_ID, create a VCPU instance for it */
#define HAX_VM_IOCTL_VCPU_CREATE    _IOR(0, 0x80, int)
/*
 * Allocate guest memory, the step of allocate guest memory is:
 * 1. QEMU will allocate the virtual address to cover the guest memory ranges
 * 2. QEMU passing down the virtual address and length in the
 *    HAX_VM_IOCTL_ALLOC_RAM ioctl through hax_alloc_ram_info structure
 * 3. HAX driver populate physical memory for the virtual address range, and
 *    lock these physical memory lock, so that they will not be swapped out
 * 4. HAX driver map the populated physical memory into kernel address space
 */
#define HAX_VM_IOCTL_ALLOC_RAM _IOWR(0, 0x81, struct hax_alloc_ram_info)
/*
 * Setup translation between guest physical address and host physical address
 */
#define HAX_VM_IOCTL_SET_RAM _IOWR(0, 0x82, struct hax_set_ram_info)

/*
 * QEMU notify HAXM driver of the API version currently in use, so that
 * HAXM driver will not present features that possibly not supported
 * by QEMU
 */
#define HAX_VM_IOCTL_NOTIFY_QEMU_VERSION   _IOW(0, 0x84, struct hax_qemu_version)

/* Run the guest in non-root mode */
#define HAX_VCPU_IOCTL_RUN  _IO(0, 0xc0)
/* Sync QEMU's guest MSR value to HAX driver */
#define HAX_VCPU_IOCTL_SET_MSRS _IOWR(0, 0xc1, struct hax_msr_data)
/* Sync HAX driver's guest MSR value to QEMU */
#define HAX_VCPU_IOCTL_GET_MSRS _IOWR(0, 0xc2, struct hax_msr_data)
#define HAX_VCPU_IOCTL_SET_FPU  _IOW(0, 0xc3, struct fx_layout)
#define HAX_VCPU_IOCTL_GET_FPU  _IOR(0, 0xc4, struct fx_layout)

/* Setup HAX tunnel, see structure hax_tunnel comments in hax-interface.h */
#define HAX_VCPU_IOCTL_SETUP_TUNNEL _IOWR(0, 0xc5, struct hax_tunnel_info)
/* A interrupt need to be injected into guest */
#define HAX_VCPU_IOCTL_INTERRUPT _IOWR(0, 0xc6, uint32_t)
#define HAX_VCPU_SET_REGS       _IOWR(0, 0xc7, struct vcpu_state_t)
#define HAX_VCPU_GET_REGS       _IOWR(0, 0xc8, struct vcpu_state_t)

#endif /* __HAX_UNIX_H */
