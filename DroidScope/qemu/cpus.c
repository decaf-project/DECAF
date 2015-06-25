/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "config-host.h"

#include "monitor.h"
#include "sysemu.h"
#include "gdbstub.h"
#include "dma.h"
#include "kvm.h"
#include "hax.h"

#include "cpus.h"

static CPUState *cur_cpu;
static CPUState *next_cpu;

/***********************************************************/
void hw_error(const char *fmt, ...)
{
    va_list ap;
    CPUState *env;

    va_start(ap, fmt);
    fprintf(stderr, "qemu: hardware error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    for(env = first_cpu; env != NULL; env = env->next_cpu) {
        fprintf(stderr, "CPU #%d:\n", env->cpu_index);
#ifdef TARGET_I386
        cpu_dump_state(env, stderr, fprintf, X86_DUMP_FPU);
#else
        cpu_dump_state(env, stderr, fprintf, 0);
#endif
    }
    va_end(ap);
    abort();
}

static void do_vm_stop(int reason)
{
    if (vm_running) {
        cpu_disable_ticks();
        vm_running = 0;
        pause_all_vcpus();
        vm_state_notify(0, reason);
    }
}

static int cpu_can_run(CPUState *env)
{
    if (env->stop)
        return 0;
    if (env->stopped)
        return 0;
    return 1;
}

static int cpu_has_work(CPUState *env)
{
    if (env->stop)
        return 1;
    if (env->stopped)
        return 0;
    if (!env->halted)
        return 1;
    if (qemu_cpu_has_work(env))
        return 1;
    return 0;
}

int tcg_has_work(void)
{
    CPUState *env;

    for (env = first_cpu; env != NULL; env = env->next_cpu)
        if (cpu_has_work(env))
            return 1;
    return 0;
}

#ifndef _WIN32
static int io_thread_fd = -1;

#if 0
static void qemu_event_increment(void)
{
    static const char byte = 0;

    if (io_thread_fd == -1)
        return;

    write(io_thread_fd, &byte, sizeof(byte));
}
#endif

static void qemu_event_read(void *opaque)
{
    int fd = (unsigned long)opaque;
    ssize_t len;

    /* Drain the notify pipe */
    do {
        char buffer[512];
        len = read(fd, buffer, sizeof(buffer));
    } while ((len == -1 && errno == EINTR) || len > 0);
}

static int qemu_event_init(void)
{
    int err;
    int fds[2];

    err = pipe(fds);
    if (err == -1)
        return -errno;

    err = fcntl_setfl(fds[0], O_NONBLOCK);
    if (err < 0)
        goto fail;

    err = fcntl_setfl(fds[1], O_NONBLOCK);
    if (err < 0)
        goto fail;

    qemu_set_fd_handler2(fds[0], NULL, qemu_event_read, NULL,
                         (void *)(unsigned long)fds[0]);

    io_thread_fd = fds[1];
    return 0;

fail:
    close(fds[0]);
    close(fds[1]);
    return err;
}
#else
HANDLE qemu_event_handle;

static void dummy_event_handler(void *opaque)
{
}

static int qemu_event_init(void)
{
    qemu_event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!qemu_event_handle) {
        perror("Failed CreateEvent");
        return -1;
    }
    qemu_add_wait_object(qemu_event_handle, dummy_event_handler, NULL);
    return 0;
}

#if 0
static void qemu_event_increment(void)
{
    SetEvent(qemu_event_handle);
}
#endif
#endif

#ifndef CONFIG_IOTHREAD
int qemu_init_main_loop(void)
{
    return qemu_event_init();
}

void qemu_init_vcpu(void *_env)
{
    CPUState *env = _env;

    if (kvm_enabled())
        kvm_init_vcpu(env);
#ifdef CONFIG_HAX
    if (hax_enabled())
        hax_init_vcpu(env);
#endif
    return;
}

int qemu_cpu_self(void *env)
{
    return 1;
}

void resume_all_vcpus(void)
{
}

void pause_all_vcpus(void)
{
}

void qemu_cpu_kick(void *env)
{
    return;
}

void qemu_notify_event(void)
{
    CPUState *env = cpu_single_env;

    if (env) {
        cpu_exit(env);
#ifdef USE_KQEMU
        if (env->kqemu_enabled)
            kqemu_cpu_interrupt(env);
#endif
    /*
     * This is mainly for the Windows host, where the timer may be in
     * a different thread with vcpu. Thus the timer function needs to
     * notify the vcpu thread of more than simply cpu_exit.  If env is
     * not NULL, it means that the vcpu is in execute state, we need
     * only to set the flags.  If the guest is in execute state, the
     * HAX kernel module will exit to qemu.  If env is NULL, vcpu is
     * in main_loop_wait, and we need a event to notify it.
     */
#ifdef CONFIG_HAX
        if (hax_enabled())
            hax_raise_event(env);
     } else {
#ifdef _WIN32
         if(hax_enabled())
             SetEvent(qemu_event_handle);
#endif
     }
#else
     }
#endif
}

void qemu_mutex_lock_iothread(void)
{
}

void qemu_mutex_unlock_iothread(void)
{
}

void vm_stop(int reason)
{
    do_vm_stop(reason);
}

#else /* CONFIG_IOTHREAD */

#include "qemu-thread.h"

QemuMutex qemu_global_mutex;
static QemuMutex qemu_fair_mutex;

static QemuThread io_thread;

static QemuThread *tcg_cpu_thread;
static QemuCond *tcg_halt_cond;

static int qemu_system_ready;
/* cpu creation */
static QemuCond qemu_cpu_cond;
/* system init */
static QemuCond qemu_system_cond;
static QemuCond qemu_pause_cond;

static void block_io_signals(void);
static void unblock_io_signals(void);
static int tcg_has_work(void);

int qemu_init_main_loop(void)
{
    int ret;

    ret = qemu_event_init();
    if (ret)
        return ret;

    qemu_cond_init(&qemu_pause_cond);
    qemu_mutex_init(&qemu_fair_mutex);
    qemu_mutex_init(&qemu_global_mutex);
    qemu_mutex_lock(&qemu_global_mutex);

    unblock_io_signals();
    qemu_thread_self(&io_thread);

    return 0;
}

static void qemu_wait_io_event(CPUState *env)
{
    while (!tcg_has_work())
        qemu_cond_timedwait(env->halt_cond, &qemu_global_mutex, 1000);

    qemu_mutex_unlock(&qemu_global_mutex);

    /*
     * Users of qemu_global_mutex can be starved, having no chance
     * to acquire it since this path will get to it first.
     * So use another lock to provide fairness.
     */
    qemu_mutex_lock(&qemu_fair_mutex);
    qemu_mutex_unlock(&qemu_fair_mutex);

    qemu_mutex_lock(&qemu_global_mutex);
    if (env->stop) {
        env->stop = 0;
        env->stopped = 1;
        qemu_cond_signal(&qemu_pause_cond);
    }
}

static int qemu_cpu_exec(CPUState *env);

static void *kvm_cpu_thread_fn(void *arg)
{
    CPUState *env = arg;

    block_io_signals();
    qemu_thread_self(env->thread);

    /* signal CPU creation */
    qemu_mutex_lock(&qemu_global_mutex);
    env->created = 1;
    qemu_cond_signal(&qemu_cpu_cond);

    /* and wait for machine initialization */
    while (!qemu_system_ready)
        qemu_cond_timedwait(&qemu_system_cond, &qemu_global_mutex, 100);

    while (1) {
        if (cpu_can_run(env))
            qemu_cpu_exec(env);
        qemu_wait_io_event(env);
    }

    return NULL;
}

static void tcg_cpu_exec(void);

static void *tcg_cpu_thread_fn(void *arg)
{
    CPUState *env = arg;

    block_io_signals();
    qemu_thread_self(env->thread);

    /* signal CPU creation */
    qemu_mutex_lock(&qemu_global_mutex);
    for (env = first_cpu; env != NULL; env = env->next_cpu)
        env->created = 1;
    qemu_cond_signal(&qemu_cpu_cond);

    /* and wait for machine initialization */
    while (!qemu_system_ready)
        qemu_cond_timedwait(&qemu_system_cond, &qemu_global_mutex, 100);

    while (1) {
        tcg_cpu_exec();
        qemu_wait_io_event(cur_cpu);
    }

    return NULL;
}

void qemu_cpu_kick(void *_env)
{
    CPUState *env = _env;
    qemu_cond_broadcast(env->halt_cond);
    if (kvm_enabled() || hax_enabled())
        qemu_thread_signal(env->thread, SIGUSR1);
}

int qemu_cpu_self(void *env)
{
    return (cpu_single_env != NULL);
}

static void cpu_signal(int sig)
{
    if (cpu_single_env)
        cpu_exit(cpu_single_env);
}

static void block_io_signals(void)
{
    sigset_t set;
    struct sigaction sigact;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpu_signal;
    sigaction(SIGUSR1, &sigact, NULL);
}

static void unblock_io_signals(void)
{
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}

static void qemu_signal_lock(unsigned int msecs)
{
    qemu_mutex_lock(&qemu_fair_mutex);

    while (qemu_mutex_trylock(&qemu_global_mutex)) {
        qemu_thread_signal(tcg_cpu_thread, SIGUSR1);
        if (!qemu_mutex_timedlock(&qemu_global_mutex, msecs))
            break;
    }
    qemu_mutex_unlock(&qemu_fair_mutex);
}

void qemu_mutex_lock_iothread(void)
{
    if (kvm_enabled() || hax_enabled()) {
        qemu_mutex_lock(&qemu_fair_mutex);
        qemu_mutex_lock(&qemu_global_mutex);
        qemu_mutex_unlock(&qemu_fair_mutex);
    } else
        qemu_signal_lock(100);
}

void qemu_mutex_unlock_iothread(void)
{
    qemu_mutex_unlock(&qemu_global_mutex);
}

static int all_vcpus_paused(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        if (!penv->stopped)
            return 0;
        penv = (CPUState *)penv->next_cpu;
    }

    return 1;
}

void pause_all_vcpus(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        penv->stop = 1;
        qemu_thread_signal(penv->thread, SIGUSR1);
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
    }

    while (!all_vcpus_paused()) {
        qemu_cond_timedwait(&qemu_pause_cond, &qemu_global_mutex, 100);
        penv = first_cpu;
        while (penv) {
            qemu_thread_signal(penv->thread, SIGUSR1);
            penv = (CPUState *)penv->next_cpu;
        }
    }
}

void resume_all_vcpus(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        penv->stop = 0;
        penv->stopped = 0;
        qemu_thread_signal(penv->thread, SIGUSR1);
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
    }
}

static void tcg_init_vcpu(void *_env)
{
    CPUState *env = _env;
    /* share a single thread for all cpus with TCG */
    if (!tcg_cpu_thread) {
        env->thread = qemu_mallocz(sizeof(QemuThread));
        env->halt_cond = qemu_mallocz(sizeof(QemuCond));
        qemu_cond_init(env->halt_cond);
        qemu_thread_create(env->thread, tcg_cpu_thread_fn, env);
        while (env->created == 0)
            qemu_cond_timedwait(&qemu_cpu_cond, &qemu_global_mutex, 100);
        tcg_cpu_thread = env->thread;
        tcg_halt_cond = env->halt_cond;
    } else {
        env->thread = tcg_cpu_thread;
        env->halt_cond = tcg_halt_cond;
    }
}

static void kvm_start_vcpu(CPUState *env)
{
#if 0
    kvm_init_vcpu(env);
    env->thread = qemu_mallocz(sizeof(QemuThread));
    env->halt_cond = qemu_mallocz(sizeof(QemuCond));
    qemu_cond_init(env->halt_cond);
    qemu_thread_create(env->thread, kvm_cpu_thread_fn, env);
    while (env->created == 0)
        qemu_cond_timedwait(&qemu_cpu_cond, &qemu_global_mutex, 100);
#endif
}

void qemu_init_vcpu(void *_env)
{
    CPUState *env = _env;

    if (kvm_enabled())
        kvm_start_vcpu(env);
    else
        tcg_init_vcpu(env);
}

void qemu_notify_event(void)
{
    qemu_event_increment();
}

void vm_stop(int reason)
{
    QemuThread me;
    qemu_thread_self(&me);

    if (!qemu_thread_equal(&me, &io_thread)) {
        qemu_system_vmstop_request(reason);
        /*
         * FIXME: should not return to device code in case
         * vm_stop() has been requested.
         */
        if (cpu_single_env) {
            cpu_exit(cpu_single_env);
            cpu_single_env->stop = 1;
        }
        return;
    }
    do_vm_stop(reason);
}

#endif

static int qemu_cpu_exec(CPUState *env)
{
    int ret;
#ifdef CONFIG_PROFILER
    int64_t ti;
#endif

#ifdef CONFIG_PROFILER
    ti = profile_getclock();
#endif
    if (use_icount) {
        int64_t count;
        int decr;
        qemu_icount -= (env->icount_decr.u16.low + env->icount_extra);
        env->icount_decr.u16.low = 0;
        env->icount_extra = 0;
        count = qemu_next_icount_deadline();
        count = (count + (1 << icount_time_shift) - 1)
                >> icount_time_shift;
        qemu_icount += count;
        decr = (count > 0xffff) ? 0xffff : count;
        count -= decr;
        env->icount_decr.u16.low = decr;
        env->icount_extra = count;
    }
#ifdef CONFIG_TRACE
    if (tbflush_requested) {
        tbflush_requested = 0;
        tb_flush(env);
        return EXCP_INTERRUPT;
    }
#endif


    ret = cpu_exec(env);
#ifdef CONFIG_PROFILER
    qemu_time += profile_getclock() - ti;
#endif
    if (use_icount) {
        /* Fold pending instructions back into the
           instruction counter, and clear the interrupt flag.  */
        qemu_icount -= (env->icount_decr.u16.low
                        + env->icount_extra);
        env->icount_decr.u32 = 0;
        env->icount_extra = 0;
    }
    return ret;
}

void tcg_cpu_exec(void)
{
    int ret = 0;

    if (next_cpu == NULL)
        next_cpu = first_cpu;
    for (; next_cpu != NULL; next_cpu = next_cpu->next_cpu) {
        CPUState *env = cur_cpu = next_cpu;

        if (!vm_running)
            break;
        if (qemu_timer_alarm_pending()) {
            break;
        }
        if (cpu_can_run(env))
            ret = qemu_cpu_exec(env);
        if (ret == EXCP_DEBUG) {
            gdb_set_stop_cpu(env);
            debug_requested = 1;
            break;
        }
    }
}

