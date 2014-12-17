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
/*
 * Virtual hardware for bridging the FUSE kernel module
 * in the emulated OS and outside file system
 */
#include "qemu_file.h"
#include "goldfish_trace.h"
#include "sysemu.h"
#include "android-trace.h"
#ifdef CONFIG_MEMCHECK
#include "memcheck/memcheck.h"
#include "memcheck/memcheck_util.h"
#endif  // CONFIG_MEMCHECK

/* Set to 1 to debug tracing */
#define DEBUG   0

#if DEBUG
#  define D(...)  printf(__VA_ARGS__), fflush(stdout)
#else
#  define D(...)  ((void)0)
#endif

/* Set to 1 to debug PID tracking */
#define  DEBUG_PID  0

#if DEBUG_PID
#  define  DPID(...)  printf(__VA_ARGS__), fflush(stdout)
#else
#  define  DPID(...)  ((void)0)
#endif

extern void cpu_loop_exit(void);

extern int tracing;
extern const char *trace_filename;

/* for execve */
static char exec_path[CLIENT_PAGE_SIZE];
static char exec_arg[CLIENT_PAGE_SIZE];
static unsigned long vstart;    // VM start
static unsigned long vend;      // VM end
static unsigned long eoff;      // offset in EXE file
static unsigned cmdlen;         // cmdline length
static unsigned pid;            // PID (really thread id)
static unsigned tgid;           // thread group id (really process id)
static unsigned tid;            // current thread id (same as pid, most of the time)
static unsigned long dsaddr;    // dynamic symbol address
static unsigned long unmap_start; // start address to unmap

/* for context switch */
//static unsigned long cs_pid;    // context switch PID

/* I/O write */
static void trace_dev_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    trace_dev_state *s = (trace_dev_state *)opaque;

    (void)s;

    switch (offset >> 2) {
    case TRACE_DEV_REG_SWITCH:  // context switch, switch to pid
        DPID("QEMU.trace: context switch tid=%u\n", value);
        if (trace_filename != NULL) {
            trace_switch(value);
            D("QEMU.trace: kernel, context switch %u\n", value);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            memcheck_switch(value);
        }
#endif  // CONFIG_MEMCHECK
        tid = (unsigned) value;
        break;
    case TRACE_DEV_REG_TGID:    // save the tgid for the following fork/clone
        DPID("QEMU.trace: tgid=%u\n", value);
        tgid = value;
        if (trace_filename != NULL) {
            D("QEMU.trace: kernel, tgid %u\n", value);
        }
        break;
    case TRACE_DEV_REG_FORK:    // fork, fork new pid
        DPID("QEMU.trace: fork (pid=%d tgid=%d value=%d)\n", pid, tgid, value);
        if (trace_filename != NULL) {
            trace_fork(tgid, value);
            D("QEMU.trace: kernel, fork %u\n", value);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            memcheck_fork(tgid, value);
        }
#endif  // CONFIG_MEMCHECK
        break;
    case TRACE_DEV_REG_CLONE:    // fork, clone new pid (i.e. thread)
        DPID("QEMU.trace: clone (pid=%d tgid=%d value=%d)\n", pid, tgid, value);
        if (trace_filename != NULL) {
            trace_clone(tgid, value);
            D("QEMU.trace: kernel, clone %u\n", value);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            memcheck_clone(tgid, value);
        }
#endif  // CONFIG_MEMCHECK
        break;
    case TRACE_DEV_REG_EXECVE_VMSTART:  // execve, vstart
        vstart = value;
        break;
    case TRACE_DEV_REG_EXECVE_VMEND:    // execve, vend
        vend = value;
        break;
    case TRACE_DEV_REG_EXECVE_OFFSET:   // execve, offset in EXE
        eoff = value;
        break;
    case TRACE_DEV_REG_EXECVE_EXEPATH:  // init exec, path of EXE
        vstrcpy(value, exec_path, CLIENT_PAGE_SIZE);
        if (trace_filename != NULL) {
            trace_init_exec(vstart, vend, eoff, exec_path);
            D("QEMU.trace: kernel, init exec [%lx,%lx]@%lx [%s]\n",
              vstart, vend, eoff, exec_path);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            if (exec_path[0] == '\0') {
                // vstrcpy may fail to copy path. In this case lets do it
                // differently.
                memcheck_get_guest_kernel_string(exec_path, value, CLIENT_PAGE_SIZE);
            }
            memcheck_mmap_exepath(vstart, vend, eoff, exec_path);
        }
#endif  // CONFIG_MEMCHECK
        exec_path[0] = 0;
        break;
    case TRACE_DEV_REG_CMDLINE_LEN:     // execve, process cmdline length
        cmdlen = value;
        break;
    case TRACE_DEV_REG_CMDLINE:         // execve, process cmdline
        cpu_memory_rw_debug(cpu_single_env, value, (uint8_t*)exec_arg, cmdlen, 0);
        if (trace_filename != NULL) {
            trace_execve(exec_arg, cmdlen);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            memcheck_set_cmd_line(exec_arg, cmdlen);
        }
#endif  // CONFIG_MEMCHECK
#if DEBUG || DEBUG_PID
        if (trace_filename != NULL) {
            int i;
            for (i = 0; i < cmdlen; i ++)
                if (i != cmdlen - 1 && exec_arg[i] == 0)
                    exec_arg[i] = ' ';
            printf("QEMU.trace: kernel, execve %s[%d]\n", exec_arg, cmdlen);
            exec_arg[0] = 0;
        }
#endif
        break;
    case TRACE_DEV_REG_EXIT:            // exit, exit current process with exit code
        DPID("QEMU.trace: exit tid=%u\n", value);
        if (trace_filename != NULL) {
            trace_exit(value);
            D("QEMU.trace: kernel, exit %x\n", value);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            memcheck_exit(value);
        }
#endif  // CONFIG_MEMCHECK
        break;
    case TRACE_DEV_REG_NAME:            // record thread name
        vstrcpy(value, exec_path, CLIENT_PAGE_SIZE);
        DPID("QEMU.trace: thread name=%s\n", exec_path);

        // Remove the trailing newline if it exists
        int len = strlen(exec_path);
        if (exec_path[len - 1] == '\n') {
            exec_path[len - 1] = 0;
        }
        if (trace_filename != NULL) {
            trace_name(exec_path);
            D("QEMU.trace: kernel, name %s\n", exec_path);
        }
        break;
    case TRACE_DEV_REG_MMAP_EXEPATH:    // mmap, path of EXE, the others are same as execve
        vstrcpy(value, exec_path, CLIENT_PAGE_SIZE);
        DPID("QEMU.trace: mmap exe=%s\n", exec_path);
        if (trace_filename != NULL) {
            trace_mmap(vstart, vend, eoff, exec_path);
            D("QEMU.trace: kernel, mmap [%lx,%lx]@%lx [%s]\n", vstart, vend, eoff, exec_path);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            if (exec_path[0] == '\0') {
                // vstrcpy may fail to copy path. In this case lets do it
                // differently.
                memcheck_get_guest_kernel_string(exec_path, value, CLIENT_PAGE_SIZE);
            }
            memcheck_mmap_exepath(vstart, vend, eoff, exec_path);
        }
#endif  // CONFIG_MEMCHECK
        exec_path[0] = 0;
        break;
    case TRACE_DEV_REG_INIT_PID:        // init, name the pid that starts before device registered
        pid = value;
        DPID("QEMU.trace: pid=%d\n", value);
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            memcheck_init_pid(value);
        }
#endif  // CONFIG_MEMCHECK
        break;
    case TRACE_DEV_REG_INIT_NAME:       // init, the comm of the init pid
        vstrcpy(value, exec_path, CLIENT_PAGE_SIZE);
        DPID("QEMU.trace: tgid=%d pid=%d name=%s\n", tgid, pid, exec_path);
        if (trace_filename != NULL) {
            trace_init_name(tgid, pid, exec_path);
            D("QEMU.trace: kernel, init name %u [%s]\n", pid, exec_path);
        }
        exec_path[0] = 0;
        break;

    case TRACE_DEV_REG_DYN_SYM_ADDR:    // dynamic symbol address
        dsaddr = value;
        break;
    case TRACE_DEV_REG_DYN_SYM:         // add dynamic symbol
        vstrcpy(value, exec_arg, CLIENT_PAGE_SIZE);
        if (trace_filename != NULL) {
            trace_dynamic_symbol_add(dsaddr, exec_arg);
            D("QEMU.trace: dynamic symbol %lx:%s\n", dsaddr, exec_arg);
        }
        exec_arg[0] = 0;
        break;
    case TRACE_DEV_REG_REMOVE_ADDR:         // remove dynamic symbol addr
        if (trace_filename != NULL) {
            trace_dynamic_symbol_remove(value);
            D("QEMU.trace: dynamic symbol remove %lx\n", dsaddr);
        }
        break;

    case TRACE_DEV_REG_PRINT_STR:       // print string
        vstrcpy(value, exec_arg, CLIENT_PAGE_SIZE);
        printf("%s", exec_arg);
        exec_arg[0] = 0;
        break;
    case TRACE_DEV_REG_PRINT_NUM_DEC:   // print number in decimal
        printf("%d", value);
        break;
    case TRACE_DEV_REG_PRINT_NUM_HEX:   // print number in hexical
        printf("%x", value);
        break;

    case TRACE_DEV_REG_STOP_EMU:        // stop the VM execution
        if (trace_filename != NULL) {
            // To ensure that the number of instructions executed in this
            // block is correct, we pretend that there was an exception.
            trace_exception(0);
        }
        cpu_single_env->exception_index = EXCP_HLT;
        cpu_single_env->halted = 1;
        qemu_system_shutdown_request();
        cpu_loop_exit();
        break;

    case TRACE_DEV_REG_ENABLE:          // tracing enable: 0 = stop, 1 = start
        if (value == 1) {
            if (trace_filename != NULL) {
                start_tracing();
            }
        }
        else if (value == 0) {
            if (trace_filename != NULL) {
                stop_tracing();

                // To ensure that the number of instructions executed in this
                // block is correct, we pretend that there was an exception.
                trace_exception(0);
            }
        }
        break;

    case TRACE_DEV_REG_UNMAP_START:
        unmap_start = value;
        break;
    case TRACE_DEV_REG_UNMAP_END:
        if (trace_filename != NULL) {
            trace_munmap(unmap_start, value);
        }
#ifdef CONFIG_MEMCHECK
        if (memcheck_enabled) {
            memcheck_unmap(unmap_start, value);
        }
#endif  // CONFIG_MEMCHECK
        break;

    case TRACE_DEV_REG_METHOD_ENTRY:
    case TRACE_DEV_REG_METHOD_EXIT:
    case TRACE_DEV_REG_METHOD_EXCEPTION:
    case TRACE_DEV_REG_NATIVE_ENTRY:
    case TRACE_DEV_REG_NATIVE_EXIT:
    case TRACE_DEV_REG_NATIVE_EXCEPTION:
        if (trace_filename != NULL) {
            if (tracing) {
                int call_type = (offset - 4096) >> 2;
                trace_interpreted_method(value, call_type);
            }
        }
        break;

#ifdef CONFIG_MEMCHECK
    case TRACE_DEV_REG_MALLOC:
        if (memcheck_enabled) {
            memcheck_guest_alloc(value);
        }
        break;

    case TRACE_DEV_REG_FREE_PTR:
        if (memcheck_enabled) {
            memcheck_guest_free(value);
        }
        break;

    case TRACE_DEV_REG_QUERY_MALLOC:
        if (memcheck_enabled) {
            memcheck_guest_query_malloc(value);
        }
        break;

    case TRACE_DEV_REG_LIBC_INIT:
        if (memcheck_enabled) {
            memcheck_guest_libc_initialized(value);
        }
        break;

    case TRACE_DEV_REG_PRINT_USER_STR:
        if (memcheck_enabled) {
            memcheck_guest_print_str(value);
        }
        break;
#endif // CONFIG_MEMCHECK

    default:
        if (offset < 4096) {
            cpu_abort(cpu_single_env, "trace_dev_write: Bad offset %x\n", offset);
        } else {
            D("%s: offset=%d (0x%x) value=%d (0x%x)\n", __FUNCTION__, offset,
              offset, value, value);
        }
        break;
    }
}

/* I/O read */
static uint32_t trace_dev_read(void *opaque, target_phys_addr_t offset)
{
    trace_dev_state *s = (trace_dev_state *)opaque;

    (void)s;

    switch (offset >> 2) {
    case TRACE_DEV_REG_ENABLE:          // tracing enable
        return tracing;

    default:
        if (offset < 4096) {
            cpu_abort(cpu_single_env, "trace_dev_read: Bad offset %x\n", offset);
        } else {
            D("%s: offset=%d (0x%x)\n", __FUNCTION__, offset, offset);
        }
        return 0;
    }
    return 0;
}

static CPUReadMemoryFunc *trace_dev_readfn[] = {
   trace_dev_read,
   trace_dev_read,
   trace_dev_read
};

static CPUWriteMemoryFunc *trace_dev_writefn[] = {
   trace_dev_write,
   trace_dev_write,
   trace_dev_write
};

/* initialize the trace device */
void trace_dev_init()
{
    trace_dev_state *s;

    s = (trace_dev_state *)qemu_mallocz(sizeof(trace_dev_state));
    s->dev.name = "qemu_trace";
    s->dev.id = -1;
    s->dev.base = 0;       // will be allocated dynamically
    s->dev.size = 0x2000;
    s->dev.irq = 0;
    s->dev.irq_count = 0;

    goldfish_device_add(&s->dev, trace_dev_readfn, trace_dev_writefn, s);

    exec_path[0] = exec_arg[0] = '\0';
}
