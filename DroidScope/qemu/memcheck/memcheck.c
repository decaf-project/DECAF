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
 * Contains implementation of memory checking framework in the emulator.
 */

#include "qemu-queue.h"
#include "qemu_file.h"
#include "elff_api.h"
#include "memcheck.h"
#include "memcheck_proc_management.h"
#include "memcheck_util.h"
#include "memcheck_logging.h"

// =============================================================================
// Global data
// =============================================================================

/* Controls what messages from the guest should be printed to emulator's
 * stdout. This variable holds a combinations of TRACE_LIBC_XXX flags. */
uint32_t trace_flags = 0;

/* Global flag, indicating whether or not memchecking has been enabled
 * for the current emulator session. 1 means that memchecking has been enabled,
 * 0 means that memchecking has not been enabled. */
int memcheck_enabled = 0;

/* Global flag, indicating whether or not __ld/__stx_mmu should be instrumented
 * for checking for access violations. If read / write access violation check
 * has been disabled by -memcheck flags, there is no need to instrument mmu
 * routines and waste performance.
 * 1 means that instrumenting is required, 0 means that instrumenting is not
 * required. */
int memcheck_instrument_mmu = 0;

/* Global flag, indicating whether or not memchecker is collecting call stack.
 * 1 - call stack is being collected, 0 means that stack is not being
 * collected. */
int memcheck_watch_call_stack = 1;

// =============================================================================
// Static routines.
// =============================================================================

/* Prints invalid pointer access violation information.
 * Param:
 *  proc - Process that caused access violation.
 *  ptr - Pointer that caused access violation.
 *  routine - If 1, access violation has occurred in 'free' routine.
 *      If 2, access violation has occurred in 'realloc' routine.
 */
static void
av_invalid_pointer(ProcDesc* proc, target_ulong ptr, int routine)
{
    if (trace_flags & TRACE_CHECK_INVALID_PTR_ENABLED) {
        printf("memcheck: Access violation is detected in process %s[pid=%u]:\n"
          "  INVALID POINTER 0x%08X is used in '%s' operation.\n"
          "  Allocation descriptor for this pointer has not been found in the\n"
          "  allocation map for the process. Most likely, this is an attempt\n"
          "  to %s a pointer that has been freed.\n",
          proc->image_path, proc->pid, ptr, routine == 1 ? "free" : "realloc",
          routine == 1 ? "free" : "reallocate");
    }
}

/* Prints read / write access violation information.
 * Param:
 *  proc - Process that caused access violation.
 *  desc - Allocation descriptor for the violation.
 *  addr - Address at which vilation has occurred.
 *  data_size - Size of data accessed at the 'addr'.
 *  val - If access violation has occurred at write operation, this parameter
 *      contains value that's being written to 'addr'. For read violation this
 *      parameter is not used.
 *  retaddr - Code address (in TB) where access violation has occurred.
 *  is_read - If 1, access violation has occurred when memory at 'addr' has been
 *      read. If 0, access violation has occurred when memory was written.
 */
static void
av_access_violation(ProcDesc* proc,
                    MallocDescEx* desc,
                    target_ulong addr,
                    uint32_t data_size,
                    uint64_t val,
                    target_ulong retaddr,
                    int is_read)
{
    target_ulong vaddr;
    Elf_AddressInfo elff_info;
    ELFF_HANDLE elff_handle = NULL;

    desc->malloc_desc.av_count++;
    if ((is_read && !(trace_flags & TRACE_CHECK_READ_VIOLATION_ENABLED)) ||
        (!is_read && !(trace_flags & TRACE_CHECK_WRITE_VIOLATION_ENABLED))) {
        return;
    }

    /* Convert host address to guest address. */
    vaddr = memcheck_tpc_to_gpc(retaddr);
    printf("memcheck: Access violation is detected in process %s[pid=%u]:\n",
           proc->image_path, proc->pid);

    /* Obtain routine, filename / line info for the address. */
    const MMRangeDesc* rdesc = procdesc_get_range_desc(proc, vaddr);
    if (rdesc != NULL) {
        int elff_res;
        printf("  In module %s at address 0x%08X\n", rdesc->path, vaddr);
        elff_res =
          memcheck_get_address_info(vaddr, rdesc, &elff_info, &elff_handle);
        if (elff_res == 0) {
            printf("  In routine %s in %s/%s:%u\n",
                   elff_info.routine_name, elff_info.dir_name,
                   elff_info.file_name, elff_info.line_number);
            if (elff_info.inline_stack != NULL) {
                const Elf_InlineInfo* inl = elff_info.inline_stack;
                int index = 0;
                for (; inl[index].routine_name != NULL; index++) {
                    char align[64];
                    size_t set_align = 4 + index * 2;
                    if (set_align >= sizeof(align)) {
                        set_align = sizeof(align) -1;
                    }
                    memset(align, ' ', set_align);
                    align[set_align] = '\0';
                    printf("%s", align);
                    if (inl[index].inlined_in_file == NULL) {
                        printf("inlined to %s in unknown location\n",
                               inl[index].routine_name);
                    } else {
                        printf("inlined to %s in %s/%s:%u\n",
                               inl[index].routine_name,
                               inl[index].inlined_in_file_dir,
                               inl[index].inlined_in_file,
                               inl[index].inlined_at_line);
                    }
                }
            }
            elff_free_pc_address_info(elff_handle, &elff_info);
            elff_close(elff_handle);
        } else if (elff_res == 1) {
            printf("  Unable to obtain routine information. Symbols file is not found.\n");
        } else {
            printf("  Unable to obtain routine information.\n"
                   "  Symbols file doesn't contain debugging information for address 0x%08X.\n",
                    mmrangedesc_get_module_offset(rdesc, vaddr));
        }
    } else {
        printf("  In unknown module at address 0x%08X\n", vaddr);
    }

    printf("  Process attempts to %s %u bytes %s address 0x%08X\n",
           is_read ? "read" : "write", data_size,
           is_read ? "from" : "to", addr);
    printf("  Accessed range belongs to the %s guarding area of allocated block.\n",
           addr < (target_ulong)mallocdesc_get_user_ptr(&desc->malloc_desc) ?
                "prefix" : "suffix");
    printf("  Allocation descriptor for this violation:\n");
    memcheck_dump_malloc_desc(desc, 1, 0);
}

/* Validates access to a guest address.
 * Param:
 *  addr - Virtual address in the guest space where memory is accessed.
 *  data_size - Size of the accessed data.
 *  proc_ptr - Upon exit from this routine contains pointer to the process
 *      descriptor for the current process, or NULL, if no such descriptor has
 *      been found.
 *  desc_ptr - Upon exit from this routine contains pointer to the allocation
 *      descriptor matching given address range, or NULL, if allocation
 *      descriptor for the validated memory range has not been found.
 * Return:
 *  0 if access to the given guest address range doesn't violate anything, or
 *  1 if given guest address range doesn't match any entry in the current
 *      process allocation descriptors map, or
 *  -1 if a violation has been detected.
 */
static int
memcheck_common_access_validation(target_ulong addr,
                                  uint32_t data_size,
                                  ProcDesc** proc_ptr,
                                  MallocDescEx** desc_ptr)
{
    MallocDescEx* desc;
    target_ulong validating_range_end;
    target_ulong user_range_end;

    ProcDesc* proc = get_current_process();
    *proc_ptr = proc;
    if (proc == NULL) {
        *desc_ptr = NULL;
        return 1;
    }

    desc = procdesc_find_malloc_for_range(proc, addr, data_size);
    *desc_ptr = desc;
    if (desc == NULL) {
        return 1;
    }

    /* Verify that validating address range doesn't start before the address
     * available to the user. */
    if (addr < mallocdesc_get_user_ptr(&desc->malloc_desc)) {
        // Stepped on the prefix guarding area.
        return -1;
    }

    validating_range_end = addr + data_size;
    user_range_end = mallocdesc_get_user_alloc_end(&desc->malloc_desc);

    /* Verify that validating address range ends inside the user block.
     * We may step on the suffix guarding area because of alignment issue.
     * For example, the application code reads last byte in the allocated block
     * with something like this:
     *
     *      char last_byte_value = *(char*)last_byte_address;
     *
     * and this code got compiled into something like this:
     *
     *      mov eax, [last_byte_address];
     *      mov [last_byte_value], al;
     *
     * In this case we will catch a read from the suffix area, even though
     * there were no errors in the code. So, in order to prevent such "false
     * negative" alarms, lets "forgive" this violation.
     * There is one bad thing about this "forgivness" though, as it may very
     * well be, that in real life some of these "out of bound" bytes will cross
     * page boundaries, marching into a page that has not been mapped to the
     * process.
     */
    if (validating_range_end <= user_range_end) {
        // Validating address range is fully contained inside the user block.
        return 0;
    }

    /* Lets see if this AV is caused by an alignment issue.*/
    if ((validating_range_end - user_range_end) < data_size) {
        /* Could be an alignment. */
        return 0;
    }

    return -1;
}

/* Checks if process has allocation descriptors for pages defined by a buffer.
 * Param:
 *  addr - Starting address of a buffer.
 *  buf_size - Buffer size.
 * Return:
 *  1 if process has allocations descriptors for pages defined by a buffer, or
 *  0 if pages containing given buffer don't have any memory allocations in
 *  them.
 */
static inline int
procdesc_contains_allocs(ProcDesc* proc, target_ulong addr, uint32_t buf_size) {
    if (proc != NULL) {
        // Beginning of the page containing last byte in range.
        const target_ulong end_page = (addr + buf_size - 1) & TARGET_PAGE_MASK;
        // Adjust beginning of the range to the beginning of the page.
        addr &= TARGET_PAGE_MASK;
        // Total size of range to check for descriptors.
        buf_size = end_page - addr + TARGET_PAGE_SIZE + 1;
        return procdesc_find_malloc_for_range(proc, addr, buf_size) ? 1 : 0;
    } else {
        return 0;
    }
}

// =============================================================================
// Memchecker API.
// =============================================================================

void
memcheck_init(const char* tracing_flags)
{
    if (*tracing_flags == '0') {
        // Memchecker is disabled.
        return;
    } else if (*tracing_flags == '1') {
        // Set default tracing.
        trace_flags = TRACE_CHECK_LEAK_ENABLED             |
                      TRACE_CHECK_READ_VIOLATION_ENABLED   |
                      TRACE_CHECK_INVALID_PTR_ENABLED      |
                      TRACE_CHECK_WRITE_VIOLATION_ENABLED;
    }

    // Parse -memcheck option params, converting them into tracing flags.
    while (*tracing_flags) {
        switch (*tracing_flags) {
            case 'A':
                // Enable all emulator's tracing messages.
                trace_flags |= TRACE_ALL_ENABLED;
                break;
            case 'F':
                // Enable fork() tracing.
                trace_flags |= TRACE_PROC_FORK_ENABLED;
                break;
            case 'S':
                // Enable guest process staring tracing.
                trace_flags |= TRACE_PROC_START_ENABLED;
                break;
            case 'E':
                // Enable guest process exiting tracing.
                trace_flags |= TRACE_PROC_EXIT_ENABLED;
                break;
            case 'C':
                // Enable clone() tracing.
                trace_flags |= TRACE_PROC_CLONE_ENABLED;
                break;
            case 'N':
                // Enable new PID allocation tracing.
                trace_flags |= TRACE_PROC_NEW_PID_ENABLED;
                break;
            case 'B':
                // Enable libc.so initialization tracing.
                trace_flags |= TRACE_PROC_LIBC_INIT_ENABLED;
                break;
            case 'L':
                // Enable memory leaks tracing.
                trace_flags |= TRACE_CHECK_LEAK_ENABLED;
                break;
            case 'I':
                // Enable invalid free / realloc pointer tracing.
                trace_flags |= TRACE_CHECK_INVALID_PTR_ENABLED;
                break;
            case 'R':
                // Enable reading violations tracing.
                trace_flags |= TRACE_CHECK_READ_VIOLATION_ENABLED;
                break;
            case 'W':
                // Enable writing violations tracing.
                trace_flags |= TRACE_CHECK_WRITE_VIOLATION_ENABLED;
                break;
            case 'M':
                // Enable module mapping tracing.
                trace_flags |= TRACE_PROC_MMAP_ENABLED;
                break;
            default:
                break;
        }
        if (trace_flags == TRACE_ALL_ENABLED) {
            break;
        }
        tracing_flags++;
    }

    /* Lets see if we need to instrument MMU, injecting memory access checking.
     * We instrument MMU only if we monitor read, or write memory access. */
    if (trace_flags & (TRACE_CHECK_READ_VIOLATION_ENABLED |
                       TRACE_CHECK_WRITE_VIOLATION_ENABLED)) {
        memcheck_instrument_mmu = 1;
    } else {
        memcheck_instrument_mmu = 0;
    }

    memcheck_init_proc_management();

    /* Lets check env. variables needed for memory checking. */
    if (getenv("ANDROID_PROJECT_OUT") == NULL) {
        printf("memcheck: Missing ANDROID_PROJECT_OUT environment variable, that is used\n"
               "to calculate path to symbol files.\n");
    }

    // Always set this flag at the very end of the initialization!
    memcheck_enabled = 1;
}

void
memcheck_guest_libc_initialized(uint32_t pid)
{
    ProcDesc* proc = get_process_from_pid(pid);
    if (proc == NULL) {
        ME("memcheck: Unable to obtain process for libc_init pid=%u", pid);
        return;
    }
    proc->flags |= PROC_FLAG_LIBC_INITIALIZED;

    /* When process initializes its own libc.so instance, it means that now
     * it has fresh heap. So, at this point we must get rid of all entries
     * (inherited and transition) that were collected in this process'
     * allocation descriptors map. */
    procdesc_empty_alloc_map(proc);
    T(PROC_LIBC_INIT, "memcheck: libc.so has been initialized for %s[pid=%u]\n",
      proc->image_path, proc->pid);
}

void
memcheck_guest_alloc(target_ulong guest_address)
{
    MallocDescEx desc;
    MallocDescEx replaced;
    RBTMapResult insert_res;
    ProcDesc* proc;
    ThreadDesc* thread;
    uint32_t indx;

    // Copy allocation descriptor from guest to emulator.
    memcheck_get_malloc_descriptor(&desc.malloc_desc, guest_address);
    desc.flags = 0;
    desc.call_stack = NULL;
    desc.call_stack_count = 0;

    proc = get_process_from_pid(desc.malloc_desc.allocator_pid);
    if (proc == NULL) {
        ME("memcheck: Unable to obtain process for allocation pid=%u",
           desc.malloc_desc.allocator_pid);
        memcheck_fail_alloc(guest_address);
        return;
    }

    if (!procdesc_is_executing(proc)) {
        desc.flags |= MDESC_FLAG_TRANSITION_ENTRY;
    }

    /* Copy thread's calling stack to the allocation descriptor. */
    thread = get_current_thread();
    desc.call_stack_count = thread->call_stack_count;
    if (desc.call_stack_count) {
        desc.call_stack = qemu_malloc(desc.call_stack_count * sizeof(target_ulong));
        if (desc.call_stack == NULL) {
            ME("memcheck: Unable to allocate %u bytes for the calling stack",
               desc.call_stack_count * sizeof(target_ulong));
            return;
        }
    }

    /* Thread's calling stack is in descending order (i.e. first entry in the
     * thread's stack is the most distant routine from the current one). On the
     * other hand, we keep calling stack entries in allocation descriptor in
     * assending order. */
    for (indx = 0; indx < thread->call_stack_count; indx++) {
        desc.call_stack[indx] =
           thread->call_stack[thread->call_stack_count - 1 - indx].call_address;
    }

    // Save malloc descriptor in the map.
    insert_res = procdesc_add_malloc(proc, &desc, &replaced);
    if (insert_res == RBT_MAP_RESULT_ENTRY_INSERTED) {
        // Invalidate TLB cache for the allocated block.
        if (memcheck_instrument_mmu) {
            invalidate_tlb_cache(desc.malloc_desc.ptr,
                                mallocdesc_get_alloc_end(&desc.malloc_desc));
        }
    } else if (insert_res == RBT_MAP_RESULT_ENTRY_REPLACED) {
        /* We don't expect to have another entry in the map that matches
         * inserting entry. This is an error condition for us, indicating
         * that we somehow lost track of memory allocations. */
        ME("memcheck: Duplicate allocation blocks:");
        if (VERBOSE_CHECK(memcheck)) {
            printf("   New block:\n");
            memcheck_dump_malloc_desc(&desc, 1, 1);
            printf("   Replaced block:\n");
            memcheck_dump_malloc_desc(&replaced, 1, 1);
        }
        if (replaced.call_stack != NULL) {
            qemu_free(replaced.call_stack);
        }
    } else {
        ME("memcheck: Unable to insert an entry to the allocation map:");
        if (VERBOSE_CHECK(memcheck)) {
            memcheck_dump_malloc_desc(&desc, 1, 1);
        }
        memcheck_fail_alloc(guest_address);
        return;
    }
}

void
memcheck_guest_free(target_ulong guest_address)
{
    MallocFree desc;
    MallocDescEx pulled;
    int pull_res;
    ProcDesc* proc;

    // Copy free descriptor from guest to emulator.
    memcheck_get_free_descriptor(&desc, guest_address);

    proc = get_process_from_pid(desc.free_pid);
    if (proc == NULL) {
        ME("memcheck: Unable to obtain process for pid=%u on free",
           desc.free_pid);
        memcheck_fail_free(guest_address);
        return;
    }

    // Pull matching entry from the map.
    pull_res = procdesc_pull_malloc(proc, desc.ptr, &pulled);
    if (pull_res) {
        av_invalid_pointer(proc, desc.ptr, 1);
        memcheck_fail_free(guest_address);
        return;
    }

    // Make sure that ptr has expected value
    if (desc.ptr != mallocdesc_get_user_ptr(&pulled.malloc_desc)) {
        if (trace_flags & TRACE_CHECK_INVALID_PTR_ENABLED) {
            printf("memcheck: Access violation is detected in process %s[pid=%u]:\n",
                   proc->image_path, proc->pid);
            printf("  INVALID POINTER 0x%08X is used in 'free' operation.\n"
                   "  This pointer is unexpected for 'free' operation, as allocation\n"
                   "  descriptor found for this pointer in the process' allocation map\n"
                   "  suggests that 0x%08X is the pointer to be used to free this block.\n"
                   "  Allocation descriptor matching the pointer:\n",
                   desc.ptr,
                   (uint32_t)mallocdesc_get_user_ptr(&pulled.malloc_desc));
            memcheck_dump_malloc_desc(&pulled, 1, 0);
        }
    }
    if (pulled.call_stack != NULL) {
        qemu_free(pulled.call_stack);
    }
}

void
memcheck_guest_query_malloc(target_ulong guest_address)
{
    MallocDescQuery qdesc;
    MallocDescEx* found;
    ProcDesc* proc;

    // Copy free descriptor from guest to emulator.
    memcheck_get_query_descriptor(&qdesc, guest_address);

    proc = get_process_from_pid(qdesc.query_pid);
    if (proc == NULL) {
        ME("memcheck: Unable to obtain process for pid=%u on query_%s",
           qdesc.query_pid, qdesc.routine == 1 ? "free" : "realloc");
        memcheck_fail_query(guest_address);
        return;
    }

    // Find allocation entry for the given address.
    found = procdesc_find_malloc(proc, qdesc.ptr);
    if (found == NULL) {
        av_invalid_pointer(proc, qdesc.ptr, qdesc.routine);
        memcheck_fail_query(guest_address);
        return;
    }

    // Copy allocation descriptor back to the guest's space.
    memcheck_set_malloc_descriptor(qdesc.desc, &found->malloc_desc);
}

void
memcheck_guest_print_str(target_ulong str) {
    char str_copy[4096];
    memcheck_get_guest_string(str_copy, str, sizeof(str_copy));
    printf("%s", str_copy);
}

/* Validates read operations, detected in __ldx_mmu routine.
 * This routine is called from __ldx_mmu wrapper implemented in
 * softmmu_template.h on condition that loading is occurring from user memory.
 * Param:
 *  addr - Virtual address in the guest space where memory is read.
 *  data_size - Size of the read.
 *  retaddr - Code address (in TB) that accesses memory.
 * Return:
 *  1 if TLB record for the accessed page should be invalidated in order to
 *  ensure that subsequent attempts to access data in this page will cause
 *  __ld/stx_mmu to be used. If memchecker is no longer interested in monitoring
 * access to this page, this routine returns 0.
 */
int
memcheck_validate_ld(target_ulong addr,
                     uint32_t data_size,
                     target_ulong retaddr)
{
    ProcDesc* proc;
    MallocDescEx* desc;

    int res = memcheck_common_access_validation(addr, data_size, &proc, &desc);
    if (res == -1) {
        av_access_violation(proc, desc, addr, data_size, 0, retaddr, 1);
        return 1;
    }

    /* Even though descriptor for the given address range has not been found,
     * we need to make sure that pages containing the given address range
     * don't contain other descriptors. */
    return res ? procdesc_contains_allocs(proc, addr, data_size) : 0;
}

/* Validates write operations, detected in __stx_mmu routine.
 * This routine is called from __stx_mmu wrapper implemented in
 * softmmu_template.h on condition that storing is occurring from user memory.
 * Param:
 *  addr - Virtual address in the guest space where memory is written.
 *  data_size - Size of the write.
 *  value - Value to be written. Note that we typecast all values to 64 bits,
 *      since this will fit all data sizes.
 *  retaddr - Code address (in TB) that accesses memory.
 * Return:
 *  1 if TLB record for the accessed page should be invalidated in order to
 *  ensure that subsequent attempts to access data in this page will cause
 *  __ld/stx_mmu to be used. If memchecker is no longer interested in monitoring
 * access to this page, this routine returns 0.
 */
int
memcheck_validate_st(target_ulong addr,
                     uint32_t data_size,
                     uint64_t value,
                     target_ulong retaddr)
{
    MallocDescEx* desc;
    ProcDesc* proc;

    int res = memcheck_common_access_validation(addr, data_size, &proc, &desc);
    if (res == -1) {
        av_access_violation(proc, desc, addr, data_size, value, retaddr, 0);
        return 1;
    }

    /* Even though descriptor for the given address range has not been found,
     * we need to make sure that pages containing the given address range
     * don't contain other descriptors. */
    return res ? procdesc_contains_allocs(proc, addr, data_size) : 0;
}

/* Checks if given address range in the context of the current process is under
 * surveillance.
 * Param:
 *  addr - Starting address of a range.
 *  size - Range size.
 * Return:
 *  boolean: 1 if address range contains memory that require access violation
 *  detection, or 0 if given address range is in no interest to the memchecker.
 */
int
memcheck_is_checked(target_ulong addr, uint32_t size) {
    return procdesc_contains_allocs(get_current_process(), addr, size) ? 1 : 0;
}
