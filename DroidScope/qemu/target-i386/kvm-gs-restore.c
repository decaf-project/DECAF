#include <sys/types.h>
#include <sys/ioctl.h>
#include "qemu-common.h"

#ifdef CONFIG_KVM_GS_RESTORE

#define INVALID_GS_REG  0xffff
#define KVM_GS_RESTORE_NODETECTED 0x1
#define KVM_GS_RESTORE_NO 0x2
#define KVM_GS_RESTORE_YES 0x3
int initial_gs = INVALID_GS_REG;
int gs_need_restore = KVM_GS_RESTORE_NODETECTED;

static void restoregs(int gs)
{
    asm("movl %0, %%gs"::"r"(gs));
}

static unsigned int _getgs()
{
    unsigned int gs = 0;
    asm("movl %%gs,%0" :"=r"(gs):);
    return gs;
}

/* No fprintf or any system call before the gs is restored successfully */
static void check_and_restore_gs(void)
{
    if (gs_need_restore == KVM_GS_RESTORE_NO)
        return;

    restoregs(initial_gs);
}

static struct sigaction old_alarm_act, old_gio_act, old_pipe_act,old_usr1_act, old_chld_act;
static void temp_sig_handler(int host_signum)
{
    /* !!! must restore gs firstly */
    check_and_restore_gs();
    switch (host_signum)
    {
    case SIGALRM:
        old_alarm_act.sa_handler(host_signum);
        break;

    case SIGIO:
        old_gio_act.sa_handler(host_signum);
        break;

    case SIGUSR1:
        old_usr1_act.sa_handler(host_signum);
        break;

    case SIGPIPE:
        old_pipe_act.sa_handler(host_signum);
        break;

    case SIGCHLD:
        old_chld_act.sa_handler(host_signum);
        break;

    default:
        fprintf(stderr, "Not take signal %x!!\n", host_signum);
        break;
    }
}

static int sig_taken = 0;
static int take_signal_handler(void)
{
    struct sigaction act;
    int ret;

    if (gs_need_restore == KVM_GS_RESTORE_NO)
        return 0;
    if (sig_taken)
        return 0;

    sig_taken = 1;
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = temp_sig_handler;
    /* Did we missed any other signal ? */
    sigaction(SIGALRM, &act, &old_alarm_act);
    sigaction(SIGIO, &act, &old_gio_act);
    sigaction(SIGUSR1, &act, &old_usr1_act);
    sigaction(SIGPIPE, &act, &old_pipe_act);
    act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &act, &old_chld_act);
    return 1;
}

int gs_base_pre_run(void)
{
    if (unlikely(initial_gs == INVALID_GS_REG) )
    {
        initial_gs = _getgs();
        /*
         * As 2.6.35-28 lucid will get correct gs but clobbered GS_BASE
         * we have to always re-write the gs base
         */
        if (initial_gs == 0x0)
            gs_need_restore = KVM_GS_RESTORE_NO;
        else
            gs_need_restore = KVM_GS_RESTORE_YES;
    }

    take_signal_handler();
    return 0;
}

int gs_base_post_run(void)
{
    check_and_restore_gs();
    return 0;
}

/*
 * ioctl may update errno, which is in thread local storage and
 * requires gs register, we have to provide our own ioctl
 * XXX should "call %%gs:$0x10" be replaced with call to vsyscall
 * page, which is more generic and clean?
 */
int no_gs_ioctl(int fd, int type, void *arg)
{
    int ret=0;

    asm(
      "movl %3, %%edx;\n"
      "movl %2, %%ecx;\n"
      "movl %1, %%ebx;\n"
      "movl $0x36, %%eax;\n"
      "call *%%gs:0x10;\n"
      "movl %%eax, %0\n"
      : "=m"(ret)
      :"m"(fd),"m"(type),"m"(arg)
      :"%edx","%ecx","%eax","%ebx"
      );

    return ret;
}

#endif

