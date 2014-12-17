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
 * Contains implementation of routines related to process management in
 * memchecker framework.
 */

#include "elff/elff_api.h"
#include "memcheck.h"
#include "memcheck_proc_management.h"
#include "memcheck_logging.h"
#include "memcheck_util.h"

/* Current thread id.
 * This value is updated with each call to memcheck_switch, saving here
 * ID of the thread that becomes current. */
static uint32_t current_tid = 0;

/* Current thread descriptor.
 * This variable is used to cache current thread descriptor. This value gets
 * initialized on "as needed" basis, when descriptor for the current thread
 * is requested for the first time.
 * Note that every time memcheck_switch routine is called, this value gets
 * NULL'ed, since another thread becomes current. */
static ThreadDesc* current_thread = NULL;

/* Current process descriptor.
 * This variable is used to cache current process descriptor. This value gets
 * initialized on "as needed" basis, when descriptor for the current process
 * is requested for the first time.
 * Note that every time memcheck_switch routine is called, this value gets
 * NULL'ed, since new thread becomes current, thus process switch may have
 * occurred as well. */
static ProcDesc*    current_process = NULL;

/* List of running processes. */
static QLIST_HEAD(proc_list, ProcDesc) proc_list;

/* List of running threads. */
static QLIST_HEAD(thread_list, ThreadDesc) thread_list;

// =============================================================================
// Static routines
// =============================================================================

/* Creates and lists thread descriptor for a new thread.
 * This routine will allocate and initialize new thread descriptor. After that
 * this routine will insert the descriptor into the global list of running
 * threads, as well as thread list in the process descriptor of the process
 * in context of which this thread is created.
 * Param:
 *  proc - Process descriptor of the process, in context of which new thread
 *      is created.
 *  tid - Thread ID of the thread that's being created.
 * Return:
 *  New thread descriptor on success, or NULL on failure.
 */
static ThreadDesc*
create_new_thread(ProcDesc* proc, uint32_t tid)
{
    ThreadDesc* new_thread = (ThreadDesc*)qemu_malloc(sizeof(ThreadDesc));
    if (new_thread == NULL) {
        ME("memcheck: Unable to allocate new thread descriptor.");
        return NULL;
    }
    new_thread->tid = tid;
    new_thread->process = proc;
    new_thread->call_stack = NULL;
    new_thread->call_stack_count = 0;
    new_thread->call_stack_max = 0;
    QLIST_INSERT_HEAD(&thread_list, new_thread, global_entry);
    QLIST_INSERT_HEAD(&proc->threads, new_thread, proc_entry);
    return new_thread;
}

/* Creates and lists process descriptor for a new process.
 * This routine will allocate and initialize new process descriptor. After that
 * this routine will create main thread descriptor for the process (with the
 * thread ID equal to the new process ID), and then new process descriptor will
 * be inserted into the global list of running processes.
 * Param:
 *  pid - Process ID of the process that's being created.
 *  parent_pid - Process ID of the parent process.
 * Return:
 *  New process descriptor on success, or NULL on failure.
 */
static ProcDesc*
create_new_process(uint32_t pid, uint32_t parent_pid)
{
    // Create and init new process descriptor.
    ProcDesc* new_proc = (ProcDesc*)qemu_malloc(sizeof(ProcDesc));
    if (new_proc == NULL) {
        ME("memcheck: Unable to allocate new process descriptor");
        return NULL;
    }
    QLIST_INIT(&new_proc->threads);
    allocmap_init(&new_proc->alloc_map);
    mmrangemap_init(&new_proc->mmrange_map);
    new_proc->pid = pid;
    new_proc->parent_pid = parent_pid;
    new_proc->image_path = NULL;
    new_proc->flags = 0;

    if (parent_pid != 0) {
        /* If new process has been forked, it inherits a copy of parent's
         * process heap, as well as parent's mmaping of loaded modules. So, on
         * fork we're required to copy parent's allocation descriptors map, as
         * well as parent's mmapping map to the new process. */
        int failed;
        ProcDesc* parent = get_process_from_pid(parent_pid);
        if (parent == NULL) {
            ME("memcheck: Unable to get parent process pid=%u for new process pid=%u",
               parent_pid, pid);
            qemu_free(new_proc);
            return NULL;
        }

        /* Copy parent's allocation map, setting "inherited" flag, and clearing
         * parent's "transition" flag in the copied entries. */
        failed = allocmap_copy(&new_proc->alloc_map, &parent->alloc_map,
                               MDESC_FLAG_INHERITED_ON_FORK,
                               MDESC_FLAG_TRANSITION_ENTRY);
        if (failed) {
            ME("memcheck: Unable to copy process' %s[pid=%u] allocation map to new process pid=%u",
               parent->image_path, parent_pid, pid);
            allocmap_empty(&new_proc->alloc_map);
            qemu_free(new_proc);
            return NULL;
        }

        // Copy parent's memory mappings map.
        failed = mmrangemap_copy(&new_proc->mmrange_map, &parent->mmrange_map);
        if (failed) {
            ME("memcheck: Unable to copy process' %s[pid=%u] mmrange map to new process pid=%u",
               parent->image_path, parent_pid, pid);
            mmrangemap_empty(&new_proc->mmrange_map);
            allocmap_empty(&new_proc->alloc_map);
            qemu_free(new_proc);
            return NULL;
        }
    }

    // Create and register main thread descriptor for new process.
    if(create_new_thread(new_proc, pid) == NULL) {
        mmrangemap_empty(&new_proc->mmrange_map);
        allocmap_empty(&new_proc->alloc_map);
        qemu_free(new_proc);
        return NULL;
    }

    // List new process.
    QLIST_INSERT_HEAD(&proc_list, new_proc, global_entry);

    return new_proc;
}

/* Finds thread descriptor for a thread id in the global list of running
 * threads.
 * Param:
 *  tid - Thread ID to look up thread descriptor for.
 * Return:
 *  Found thread descriptor, or NULL if thread descriptor has not been found.
 */
static ThreadDesc*
get_thread_from_tid(uint32_t tid)
{
    ThreadDesc* thread;

    /* There is a pretty good chance that when this call is made, it's made
     * to get descriptor for the current thread. Lets see if it is so, so
     * we don't have to iterate through the entire list. */
    if (tid == current_tid && current_thread != NULL) {
        return current_thread;
    }

    QLIST_FOREACH(thread, &thread_list, global_entry) {
        if (tid == thread->tid) {
            if (tid == current_tid) {
                current_thread = thread;
            }
            return thread;
        }
    }
    return NULL;
}

/* Gets thread descriptor for the current thread.
 * Return:
 *  Found thread descriptor, or NULL if thread descriptor has not been found.
 */
ThreadDesc*
get_current_thread(void)
{
    // Lets see if current thread descriptor has been cached.
    if (current_thread == NULL) {
        /* Descriptor is not cached. Look it up in the list. Note that
         * get_thread_from_tid(current_tid) is not used here in order to
         * optimize this code for performance, as this routine is called from
         * the performance sensitive path. */
        ThreadDesc* thread;
        QLIST_FOREACH(thread, &thread_list, global_entry) {
            if (current_tid == thread->tid) {
                current_thread = thread;
                return current_thread;
            }
        }
    }
    return current_thread;
}

/* Finds process descriptor for a thread id.
 * Param:
 *  tid - Thread ID to look up process descriptor for.
 * Return:
 *  Process descriptor for the thread, or NULL, if process descriptor
 *  has not been found.
 */
static inline ProcDesc*
get_process_from_tid(uint32_t tid)
{
    const ThreadDesc* thread = get_thread_from_tid(tid);
    return (thread != NULL) ? thread->process : NULL;
}

/* Sets, or replaces process image path in process descriptor.
 * Generally, new process' image path is unknown untill we calculate it in
 * the handler for TRACE_DEV_REG_CMDLINE event. This routine is called from
 * TRACE_DEV_REG_CMDLINE event handler to set, or replace process image path.
 * Param:
 *  proc - Descriptor of the process where to set, or replace image path.
 *  image_path - Image path to the process, transmitted with
 *      TRACE_DEV_REG_CMDLINE event.
 * set_flags_on_replace - Flags to be set when current image path for the
 *      process has been actually replaced with the new one.
 * Return:
 *  Zero on success, or -1 on failure.
 */
static int
procdesc_set_image_path(ProcDesc* proc,
                        const char* image_path,
                        uint32_t set_flags_on_replace)
{
    if (image_path == NULL || proc == NULL) {
        return 0;
    }

    if (proc->image_path != NULL) {
        /* Process could have been forked, and inherited image path of the
         * parent process. However, it seems that "fork" in terms of TRACE_XXX
         * is not necessarly a strict "fork", but rather new process creation
         * in general. So, if that's the case we need to override image path
         * inherited from the parent process. */
        if (!strcmp(proc->image_path, image_path)) {
            // Paths are the same. Just bail out.
            return 0;
        }
        qemu_free(proc->image_path);
        proc->image_path = NULL;
    }

    // Save new image path into process' descriptor.
    proc->image_path = qemu_malloc(strlen(image_path) + 1);
    if (proc->image_path == NULL) {
        ME("memcheck: Unable to allocate %u bytes for image path %s to set it for pid=%u",
           strlen(image_path) + 1, image_path, proc->pid);
        return -1;
    }
    strcpy(proc->image_path, image_path);
    proc->flags |= set_flags_on_replace;
    return 0;
}

/* Frees thread descriptor. */
static void
threaddesc_free(ThreadDesc* thread)
{
    uint32_t indx;

    if (thread == NULL) {
        return;
    }

    if (thread->call_stack != NULL) {
        for (indx = 0; indx < thread->call_stack_count; indx++) {
            if (thread->call_stack[indx].module_path != NULL) {
                qemu_free(thread->call_stack[indx].module_path);
            }
        }
        qemu_free(thread->call_stack);
    }
    qemu_free(thread);
}

// =============================================================================
// Process management API
// =============================================================================

void
memcheck_init_proc_management(void)
{
    QLIST_INIT(&proc_list);
    QLIST_INIT(&thread_list);
}

ProcDesc*
get_process_from_pid(uint32_t pid)
{
    ProcDesc* proc;

    /* Chances are that pid addresses the current process. Lets check this,
     * so we don't have to iterate through the entire project list. */
    if (current_thread != NULL && current_thread->process->pid == pid) {
        current_process = current_thread->process;
        return current_process;
    }

    QLIST_FOREACH(proc, &proc_list, global_entry) {
        if (pid == proc->pid) {
            break;
        }
    }
    return proc;
}

ProcDesc*
get_current_process(void)
{
    if (current_process == NULL) {
        const ThreadDesc* cur_thread = get_current_thread();
        if (cur_thread != NULL) {
            current_process = cur_thread->process;
        }
    }
    return current_process;
}

void
memcheck_on_call(target_ulong from, target_ulong ret)
{
    const uint32_t grow_by = 32;
    const uint32_t max_stack = grow_by;
    ThreadDesc* thread = get_current_thread();
    if (thread == NULL) {
        return;
    }

    /* We're not saving call stack until process starts execution. */
    if (!procdesc_is_executing(thread->process)) {
        return;
    }

    const MMRangeDesc* rdesc = procdesc_get_range_desc(thread->process, from);
    if (rdesc == NULL) {
        ME("memcheck: Unable to find mapping for guest PC 0x%08X in process %s[pid=%u]",
           from, thread->process->image_path, thread->process->pid);
        return;
    }

    /* Limit calling stack size. There are cases when calling stack can be
     * quite deep due to recursion (up to 4000 entries). */
    if (thread->call_stack_count >= max_stack) {
#if 0
        /* This happens quite often. */
        MD("memcheck: Thread stack for %s[pid=%u, tid=%u] is too big: %u",
           thread->process->image_path, thread->process->pid, thread->tid,
           thread->call_stack_count);
#endif
        return;
    }

    if (thread->call_stack_count >= thread->call_stack_max) {
        /* Expand calling stack array buffer. */
        thread->call_stack_max += grow_by;
        ThreadCallStackEntry* new_array =
            qemu_malloc(thread->call_stack_max * sizeof(ThreadCallStackEntry));
        if (new_array == NULL) {
            ME("memcheck: Unable to allocate %u bytes for calling stack.",
               thread->call_stack_max * sizeof(ThreadCallStackEntry));
            thread->call_stack_max -= grow_by;
            return;
        }
        if (thread->call_stack_count != 0) {
            memcpy(new_array, thread->call_stack,
                   thread->call_stack_count * sizeof(ThreadCallStackEntry));
        }
        if (thread->call_stack != NULL) {
            qemu_free(thread->call_stack);
        }
        thread->call_stack = new_array;
    }
    thread->call_stack[thread->call_stack_count].call_address = from;
    thread->call_stack[thread->call_stack_count].call_address_rel =
            mmrangedesc_get_module_offset(rdesc, from);
    thread->call_stack[thread->call_stack_count].ret_address = ret;
    thread->call_stack[thread->call_stack_count].ret_address_rel =
            mmrangedesc_get_module_offset(rdesc, ret);
    thread->call_stack[thread->call_stack_count].module_path =
            qemu_malloc(strlen(rdesc->path) + 1);
    if (thread->call_stack[thread->call_stack_count].module_path == NULL) {
        ME("memcheck: Unable to allocate %u bytes for module path in the thread calling stack.",
            strlen(rdesc->path) + 1);
        return;
    }
    strcpy(thread->call_stack[thread->call_stack_count].module_path,
           rdesc->path);
    thread->call_stack_count++;
}

void
memcheck_on_ret(target_ulong ret)
{
    ThreadDesc* thread = get_current_thread();
    if (thread == NULL) {
        return;
    }

    /* We're not saving call stack until process starts execution. */
    if (!procdesc_is_executing(thread->process)) {
        return;
    }

    if (thread->call_stack_count > 0) {
        int indx = (int)thread->call_stack_count - 1;
        for (; indx >= 0; indx--) {
            if (thread->call_stack[indx].ret_address == ret) {
                thread->call_stack_count = indx;
                return;
            }
        }
    }
}

// =============================================================================
// Handlers for events, generated by the kernel.
// =============================================================================

void
memcheck_init_pid(uint32_t new_pid)
{
    create_new_process(new_pid, 0);
    T(PROC_NEW_PID, "memcheck: init_pid(pid=%u) in current thread tid=%u\n",
      new_pid, current_tid);
}

void
memcheck_switch(uint32_t tid)
{
    /* Since new thread became active, we have to invalidate cached
     * descriptors for current thread and process. */
    current_thread = NULL;
    current_process = NULL;
    current_tid = tid;
}

void
memcheck_fork(uint32_t tgid, uint32_t new_pid)
{
    ProcDesc* parent_proc;
    ProcDesc* new_proc;

    /* tgid may match new_pid, in which case current process is the
     * one that's being forked, otherwise tgid identifies process
     * that's being forked. */
    if (new_pid == tgid) {
        parent_proc = get_current_process();
    } else {
        parent_proc = get_process_from_tid(tgid);
    }

    if (parent_proc == NULL) {
        ME("memcheck: FORK(%u, %u): Unable to look up parent process. Current tid=%u",
           tgid, new_pid, current_tid);
        return;
    }

    if (parent_proc->pid != get_current_process()->pid) {
        MD("memcheck: FORK(%u, %u): parent %s[pid=%u] is not the current process %s[pid=%u]",
           tgid, new_pid, parent_proc->image_path, parent_proc->pid,
           get_current_process()->image_path, get_current_process()->pid);
    }

    new_proc = create_new_process(new_pid, parent_proc->pid);
    if (new_proc == NULL) {
        return;
    }

    /* Since we're possibly forking parent process, we need to inherit
     * parent's image path in the forked process. */
    procdesc_set_image_path(new_proc, parent_proc->image_path, 0);

    T(PROC_FORK, "memcheck: FORK(tgid=%u, new_pid=%u) by %s[pid=%u] (tid=%u)\n",
      tgid, new_pid, parent_proc->image_path, parent_proc->pid, current_tid);
}

void
memcheck_clone(uint32_t tgid, uint32_t new_tid)
{
    ProcDesc* parent_proc;

    /* tgid may match new_pid, in which case current process is the
     * one that creates thread, otherwise tgid identifies process
     * that creates thread. */
    if (new_tid == tgid) {
        parent_proc = get_current_process();
    } else {
        parent_proc = get_process_from_tid(tgid);
    }

    if (parent_proc == NULL) {
        ME("memcheck: CLONE(%u, %u) Unable to look up parent process. Current tid=%u",
           tgid, new_tid, current_tid);
        return;
    }

    if (parent_proc->pid != get_current_process()->pid) {
        ME("memcheck: CLONE(%u, %u): parent %s[pid=%u] is not the current process %s[pid=%u]",
           tgid, new_tid, parent_proc->image_path, parent_proc->pid,
           get_current_process()->image_path, get_current_process()->pid);
    }

    create_new_thread(parent_proc, new_tid);

    T(PROC_CLONE, "memcheck: CLONE(tgid=%u, new_tid=%u) by %s[pid=%u] (tid=%u)\n",
      tgid, new_tid, parent_proc->image_path, parent_proc->pid, current_tid);
}

void
memcheck_set_cmd_line(const char* cmd_arg, unsigned cmdlen)
{
    char parsed[4096];
    int n;

    ProcDesc* current_proc = get_current_process();
    if (current_proc == NULL) {
        ME("memcheck: CMDL(%s, %u): Unable to look up process for current tid=%3u",
           cmd_arg, cmdlen, current_tid);
        return;
    }

    /* Image path is the first agrument in cmd line. Note that due to
     * limitations of TRACE_XXX cmdlen can never exceed CLIENT_PAGE_SIZE */
    memcpy(parsed, cmd_arg, cmdlen);

    // Cut first argument off the entire command line.
    for (n = 0; n < cmdlen; n++) {
        if (parsed[n] == ' ') {
            break;
        }
    }
    parsed[n] = '\0';

    // Save process' image path into descriptor.
    procdesc_set_image_path(current_proc, parsed,
                            PROC_FLAG_IMAGE_PATH_REPLACED);
    current_proc->flags |= PROC_FLAG_EXECUTING;

    /* At this point we need to discard memory mappings inherited from
     * the parent process, since this process has become "independent" from
     * its parent. */
    mmrangemap_empty(&current_proc->mmrange_map);
    T(PROC_START, "memcheck: Executing process %s[pid=%u]\n",
      current_proc->image_path, current_proc->pid);
}

void
memcheck_exit(uint32_t exit_code)
{
    ProcDesc* proc;
    int leaks_reported = 0;
    MallocDescEx leaked_alloc;

    // Exiting thread descriptor.
    ThreadDesc* thread = get_current_thread();
    if (thread == NULL) {
        ME("memcheck: EXIT(%u): Unable to look up thread for current tid=%u",
           exit_code, current_tid);
        return;
    }
    proc = thread->process;

    // Since current thread is exiting, we need to NULL its cached descriptor.
    current_thread = NULL;

    // Unlist the thread from its process as well as global lists.
    QLIST_REMOVE(thread, proc_entry);
    QLIST_REMOVE(thread, global_entry);
    threaddesc_free(thread);

    /* Lets see if this was last process thread, which would indicate
     * process termination. */
    if (!QLIST_EMPTY(&proc->threads)) {
        return;
    }

    // Process is terminating. Report leaks and free resources.
    proc->flags |= PROC_FLAG_EXITING;

    /* Empty allocation descriptors map for the exiting process,
     * reporting leaking blocks in the process. */
    while (!allocmap_pull_first(&proc->alloc_map, &leaked_alloc)) {
        /* We should "forgive" blocks that were inherited from the
         * parent process on fork, or were allocated while process was
         * in "transition" state. */
        if (!mallocdescex_is_inherited_on_fork(&leaked_alloc) &&
            !mallocdescex_is_transition_entry(&leaked_alloc)) {
            if (!leaks_reported) {
                // First leak detected. Print report's header.
                T(CHECK_LEAK, "memcheck: Process %s[pid=%u] is exiting leaking allocated blocks:\n",
                  proc->image_path, proc->pid);
            }
            if (trace_flags & TRACE_CHECK_LEAK_ENABLED) {
                // Dump leaked block information.
                printf("   Leaked block %u:\n", leaks_reported + 1);
                memcheck_dump_malloc_desc(&leaked_alloc, 0, 0);
                if (leaked_alloc.call_stack != NULL) {
                    const int max_stack = 24;
                    if (max_stack >= leaked_alloc.call_stack_count) {
                        printf("      Call stack:\n");
                    } else {
                        printf("      Call stack (first %u of %u entries):\n",
                               max_stack, leaked_alloc.call_stack_count);
                    }
                    uint32_t stk;
                    for (stk = 0;
                         stk < leaked_alloc.call_stack_count && stk < max_stack;
                         stk++) {
                        const MMRangeDesc* rdesc =
                           procdesc_find_mapentry(proc,
                                                  leaked_alloc.call_stack[stk]);
                        if (rdesc != NULL) {
                            Elf_AddressInfo elff_info;
                            ELFF_HANDLE elff_handle = NULL;
                            uint32_t rel =
                                mmrangedesc_get_module_offset(rdesc,
                                                  leaked_alloc.call_stack[stk]);
                            printf("         Frame %u: PC=0x%08X (relative 0x%08X) in module %s\n",
                                   stk, leaked_alloc.call_stack[stk], rel,
                                   rdesc->path);
                            if (memcheck_get_address_info(leaked_alloc.call_stack[stk],
                                                          rdesc, &elff_info,
                                                          &elff_handle) == 0) {
                                printf("            Routine %s @ %s/%s:%u\n",
                                       elff_info.routine_name,
                                       elff_info.dir_name,
                                       elff_info.file_name,
                                       elff_info.line_number);
                                elff_free_pc_address_info(elff_handle,
                                                          &elff_info);
                                elff_close(elff_handle);
                            }
                        } else {
                            printf("         Frame %u: PC=0x%08X in module <unknown>\n",
                                   stk, leaked_alloc.call_stack[stk]);

                        }
                    }
                }
            }
            leaks_reported++;
        }
    }

    if (leaks_reported) {
        T(CHECK_LEAK, "memcheck: Process %s[pid=%u] is leaking %u allocated blocks.\n",
          proc->image_path, proc->pid, leaks_reported);
    }

    T(PROC_EXIT, "memcheck: Exiting process %s[pid=%u] in thread %u. Memory leaks detected: %u\n",
      proc->image_path, proc->pid, current_tid, leaks_reported);

    /* Since current process is exiting, we need to NULL its cached descriptor,
     * and unlist it from the list of running processes. */
    current_process = NULL;
    QLIST_REMOVE(proc, global_entry);

    // Empty process' mmapings map.
    mmrangemap_empty(&proc->mmrange_map);
    if (proc->image_path != NULL) {
        qemu_free(proc->image_path);
    }
    qemu_free(proc);
}

void
memcheck_mmap_exepath(target_ulong vstart,
                      target_ulong vend,
                      target_ulong exec_offset,
                      const char* path)
{
    MMRangeDesc desc;
    MMRangeDesc replaced;
    RBTMapResult ins_res;

    ProcDesc* proc = get_current_process();
    if (proc == NULL) {
        ME("memcheck: MMAP(0x%08X, 0x%08X, 0x%08X, %s) Unable to look up current process. Current tid=%u",
           vstart, vend, exec_offset, path, current_tid);
        return;
    }

    /* First, unmap an overlapped section */
    memcheck_unmap(vstart, vend);

    /* Add new mapping. */
    desc.map_start = vstart;
    desc.map_end = vend;
    desc.exec_offset = exec_offset;
    desc.path = qemu_malloc(strlen(path) + 1);
    if (desc.path == NULL) {
        ME("memcheck: MMAP(0x%08X, 0x%08X, 0x%08X, %s) Unable to allocate path for the entry.",
           vstart, vend, exec_offset, path);
        return;
    }
    strcpy(desc.path, path);

    ins_res = mmrangemap_insert(&proc->mmrange_map, &desc, &replaced);
    if (ins_res == RBT_MAP_RESULT_ERROR) {
        ME("memcheck: %s[pid=%u] unable to insert memory mapping entry: 0x%08X - 0x%08X",
           proc->image_path, proc->pid, vstart, vend);
        qemu_free(desc.path);
        return;
    }

    if (ins_res == RBT_MAP_RESULT_ENTRY_REPLACED) {
        MD("memcheck: %s[pid=%u] MMRANGE %s[0x%08X - 0x%08X] is replaced with %s[0x%08X - 0x%08X]",
           proc->image_path, proc->pid, replaced.path, replaced.map_start,
           replaced.map_end, desc.path, desc.map_start, desc.map_end);
        qemu_free(replaced.path);
    }

    T(PROC_MMAP, "memcheck: %s[pid=%u] %s is mapped: 0x%08X - 0x%08X + 0x%08X\n",
      proc->image_path, proc->pid, path, vstart, vend, exec_offset);
}

void
memcheck_unmap(target_ulong vstart, target_ulong vend)
{
    MMRangeDesc desc;
    ProcDesc* proc = get_current_process();
    if (proc == NULL) {
        ME("memcheck: UNMAP(0x%08X, 0x%08X) Unable to look up current process. Current tid=%u",
           vstart, vend, current_tid);
        return;
    }

    if (mmrangemap_pull(&proc->mmrange_map, vstart, vend, &desc)) {
        return;
    }

    if (desc.map_start >= vstart && desc.map_end <= vend) {
        /* Entire mapping has been deleted. */
        T(PROC_MMAP, "memcheck: %s[pid=%u] %s is unmapped: [0x%08X - 0x%08X + 0x%08X]\n",
          proc->image_path, proc->pid, desc.path, vstart, vend, desc.exec_offset);
        qemu_free(desc.path);
        return;
    }

    /* This can be first stage of "remap" request, when part of the existing
     * mapping has been unmapped. If that's so, lets cut unmapped part from the
     * block that we just pulled, and add whatever's left back to the map. */
    T(PROC_MMAP, "memcheck: REMAP(0x%08X, 0x%08X + 0x%08X) -> (0x%08X, 0x%08X)\n",
       desc.map_start, desc.map_end, desc.exec_offset, vstart, vend);
    if (desc.map_start == vstart) {
        /* We cut part from the beginning. Add the tail back. */
        desc.exec_offset += vend - desc.map_start;
        desc.map_start = vend;
        mmrangemap_insert(&proc->mmrange_map, &desc, NULL);
    } else if (desc.map_end == vend) {
        /* We cut part from the tail. Add the beginning back. */
        desc.map_end = vstart;
        mmrangemap_insert(&proc->mmrange_map, &desc, NULL);
    } else {
        /* We cut piece in the middle. */
        MMRangeDesc tail;
        tail.map_start = vend;
        tail.map_end = desc.map_end;
        tail.exec_offset = vend - desc.map_start + desc.exec_offset;
        tail.path = qemu_malloc(strlen(desc.path) + 1);
        strcpy(tail.path, desc.path);
        mmrangemap_insert(&proc->mmrange_map, &tail, NULL);
        desc.map_end = vstart;
        mmrangemap_insert(&proc->mmrange_map, &desc, NULL);
    }
}
