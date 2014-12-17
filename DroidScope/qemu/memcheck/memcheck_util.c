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
 * Contains implementation of utility routines for memchecker framework.
 */

#include "stdio.h"
#include "qemu-common.h"
#include "android/utils/path.h"
#include "cpu.h"
#include "memcheck_util.h"
#include "memcheck_proc_management.h"
#include "memcheck_logging.h"
//#include "softmmu_outside_jit.h"

/* Gets symblos file path for the given module.
 * Param:
 *  module_path - Path to the module to get sympath for.
 *  sym_path - Buffer, where to save path to the symbols file path for the givem
 *      module. NOTE: This buffer must be big enough to contain the largest
 *      path possible.
 *  max_char - Character size of the buffer addressed by sym_path parameter.
 * Return:
 *  0 on success, or -1 if symbols file has not been found, or sym_path buffer
 *  was too small to contain entire path.
 */
static int
get_sym_path(const char* module_path, char* sym_path, size_t max_char)
{
    const char* sym_path_root = getenv("ANDROID_PROJECT_OUT");
    if (sym_path_root == NULL || strlen(sym_path_root) >= max_char) {
        return -1;
    }

    strcpy(sym_path, sym_path_root);
    max_char -= strlen(sym_path_root);
    if (sym_path[strlen(sym_path)-1] != PATH_SEP_C) {
        strcat(sym_path, PATH_SEP);
        max_char--;
    }
    if (strlen("symbols") >= max_char) {
        return -1;
    }
    strcat(sym_path, "symbols");
    max_char -= strlen("symbols");
    if (strlen(module_path) >= max_char) {
        return -1;
    }
    strcat(sym_path, module_path);

    /* Sometimes symbol file for a module is placed into a parent symbols
     * directory. Lets iterate through all parent sym dirs, until we find
     * sym file, or reached symbols root. */
    while (!path_exists(sym_path)) {
        /* Select module name. */
        char* name = strrchr(sym_path, PATH_SEP_C);
        assert(name != NULL);
        *name = '\0';
        /* Parent directory. */
        char* parent = strrchr(sym_path, PATH_SEP_C);
        assert(parent != NULL);
        *parent = '\0';
        if (strcmp(sym_path, sym_path_root) == 0) {
            return -1;
        }
        *parent = PATH_SEP_C;
        memmove(parent+1, name + 1, strlen(name + 1) + 1);
    }

    return 0;
}

// =============================================================================
// Transfering data between guest and emulator address spaces.
// =============================================================================

void
memcheck_get_guest_buffer(void* qemu_address,
                          target_ulong guest_address,
                          size_t buffer_size)
{
    /* Byte-by-byte copying back and forth between guest's and emulator's memory
     * appears to be efficient enough (at least on small blocks used in
     * memchecker), so there is no real need to optimize it by aligning guest
     * buffer to 32 bits and use ld/stl_user instead of ld/stub_user to
     * read / write guest's memory. */
    while (buffer_size) {
        *(uint8_t*)qemu_address = ldub_user(guest_address);
        qemu_address = (uint8_t*)qemu_address + 1;
        guest_address++;
        buffer_size--;
    }
}

void
memcheck_set_guest_buffer(target_ulong guest_address,
                          const void* qemu_address,
                          size_t buffer_size)
{
    while (buffer_size) {
        stb_user(guest_address, *(uint8_t*)qemu_address);
        guest_address++;
        qemu_address = (uint8_t*)qemu_address + 1;
        buffer_size--;
    }
}

size_t
memcheck_get_guest_string(char* qemu_str,
                          target_ulong guest_str,
                          size_t qemu_buffer_size)
{
    size_t copied = 0;

    if (qemu_buffer_size > 1) {
        for (copied = 0; copied < qemu_buffer_size - 1; copied++) {
            qemu_str[copied] = ldub_user(guest_str + copied);
            if (qemu_str[copied] == '\0') {
                return copied;
            }
        }
    }
    qemu_str[copied] = '\0';
    return copied;
}

size_t
memcheck_get_guest_kernel_string(char* qemu_str,
                                 target_ulong guest_str,
                                 size_t qemu_buffer_size)
{
    size_t copied = 0;

    if (qemu_buffer_size > 1) {
        for (copied = 0; copied < qemu_buffer_size - 1; copied++) {
            qemu_str[copied] = ldub_kernel(guest_str + copied);
            if (qemu_str[copied] == '\0') {
                return copied;
            }
        }
    }
    qemu_str[copied] = '\0';
    return copied;
}

// =============================================================================
// Helpers for transfering memory allocation information.
// =============================================================================

void
memcheck_fail_alloc(target_ulong guest_address)
{
    stl_user(ALLOC_RES_ADDRESS(guest_address), 0);
}

void
memcheck_fail_free(target_ulong guest_address)
{
    stl_user(FREE_RES_ADDRESS(guest_address), 0);
}

void
memcheck_fail_query(target_ulong guest_address)
{
    stl_user(QUERY_RES_ADDRESS(guest_address), 0);
}

// =============================================================================
// Misc. utility routines.
// =============================================================================

void
invalidate_tlb_cache(target_ulong start, target_ulong end)
{
    target_ulong index = (start >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    const target_ulong to = ((end - 1) >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE-1);
    for (; index <= to; index++, start += TARGET_PAGE_SIZE) {
        target_ulong tlb_addr = cpu_single_env->tlb_table[1][index].addr_write;
        if ((start & TARGET_PAGE_MASK) ==
            (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
            cpu_single_env->tlb_table[1][index].addr_write ^= TARGET_PAGE_MASK;
        }
        tlb_addr = cpu_single_env->tlb_table[1][index].addr_read;
        if ((start & TARGET_PAGE_MASK) ==
            (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
            cpu_single_env->tlb_table[1][index].addr_read ^= TARGET_PAGE_MASK;
        }
    }
}

void
memcheck_dump_malloc_desc(const MallocDescEx* desc_ex,
                          int print_flags,
                          int print_proc_info)
{
    const MallocDesc* desc = &desc_ex->malloc_desc;
    printf("            User range:             0x%08X - 0x%08X, %u bytes\n",
           (uint32_t)mallocdesc_get_user_ptr(desc),
            (uint32_t)mallocdesc_get_user_ptr(desc) + desc->requested_bytes,
           desc->requested_bytes);
    printf("            Prefix guarding area:   0x%08X - 0x%08X, %u bytes\n",
           desc->ptr, desc->ptr + desc->prefix_size, desc->prefix_size);
    printf("            Suffix guarding area:   0x%08X - 0x%08X, %u bytes\n",
           mallocdesc_get_user_alloc_end(desc),
           mallocdesc_get_user_alloc_end(desc) + desc->suffix_size,
           desc->suffix_size);
    if (print_proc_info) {
        ProcDesc* proc = get_process_from_pid(desc->allocator_pid);
        if (proc != NULL) {
            printf("            Allocated by:           %s[pid=%u]\n",
                   proc->image_path, proc->pid);
        }
    }
    if (print_flags) {
        printf("            Flags:                  0x%08X\n", desc_ex->flags);
    }
}

int
memcheck_get_address_info(target_ulong abs_pc,
                          const MMRangeDesc* rdesc,
                          Elf_AddressInfo* info,
                          ELFF_HANDLE* elff_handle)
{
    char sym_path[MAX_PATH];
    ELFF_HANDLE handle;

    if (get_sym_path(rdesc->path, sym_path, MAX_PATH)) {
        return 1;
    }

    handle = elff_init(sym_path);
    if (handle == NULL) {
        return -1;
    }

    if (!elff_is_exec(handle)) {
        /* Debug info for shared library is created for the relative address. */
        target_ulong rel_pc = mmrangedesc_get_module_offset(rdesc, abs_pc);
        if (elff_get_pc_address_info(handle, rel_pc, info)) {
            elff_close(handle);
            return -1;
        }
    } else {
        /* Debug info for executables is created for the absoulte address. */
        if (elff_get_pc_address_info(handle, abs_pc, info)) {
            elff_close(handle);
            return -1;
        }
    }

    *elff_handle = handle;
    return 0;
}
