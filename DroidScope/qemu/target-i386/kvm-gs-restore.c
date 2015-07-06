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

struct sigact_status
{
    unsigned int sigaction:1;
    __sighandler_t old_handler;
    void (*old_sigaction) (int, siginfo_t *, void *);
};
static struct sigact_status o_sigact[SIGUNUSED];

static void temp_sig_handler(int signum)
{
    /* !!! must restore gs firstly */
    check_and_restore_gs();

    if (signum < SIGHUP || signum >= SIGUNUSED)
    {
        fprintf(stderr, "Invalid signal %x in temp_sig_handler\n", signum);
        abort();
    }

    if ( !o_sigact[signum].sigaction && o_sigact[signum].old_handler)
        o_sigact[signum].old_handler(signum);
    else
    {
        fprintf(stderr, "Invalid signal in temp_sig_handler: "
             "signal %x sa_info %s!!\n",
             signum, o_sigact[signum].sigaction ? "set":"not set" );
         abort();
    }
}

static void temp_sig_sigaction(int signum, siginfo_t *info, void *ucontext)
{
    /* !!! must restore gs firstly */
    check_and_restore_gs();

    if (signum < SIGHUP || signum >= SIGUNUSED)
    {
        fprintf(stderr, "Invalid signal %x in temp_sig_sigaction\n", signum);
        abort();
    }

    if ( o_sigact[signum].sigaction && o_sigact[signum].old_sigaction )
        o_sigact[signum].old_sigaction(signum, info, ucontext);
    else
    {
        fprintf(stderr, "Invalid signal in temp_sig_sigaction: "
             "signal %x sa_info %s!!\n",
             signum, o_sigact[signum].sigaction ? "set":"not set" );
         abort();
    }
}

static int sig_taken = 0;

static int take_signal_handler(void)
{
    int i;

    if (gs_need_restore == KVM_GS_RESTORE_NO)
        return 0;
    if (sig_taken)
        return 0;

    memset(o_sigact, 0, sizeof(o_sigact));

    /* SIGHUP is 1 in POSIX */
    for (i = SIGHUP; i < SIGUNUSED; i++)
    {
        int sigret;
        struct sigaction act, old_act;

        sigret = sigaction(i, NULL, &old_act);
        if (sigret)
            continue;
        /* We don't need take the handler for default or ignore signals */
        if ( !(old_act.sa_flags & SA_SIGINFO) &&
               ((old_act.sa_handler == SIG_IGN ) ||
                (old_act.sa_handler == SIG_DFL)))
            continue;

        memcpy(&act, &old_act, sizeof(struct sigaction));

        if (old_act.sa_flags & SA_SIGINFO)
        {
            o_sigact[i].old_sigaction = old_act.sa_sigaction;
            o_sigact[i].sigaction = 1;
            act.sa_sigaction = temp_sig_sigaction;
        }
        else
        {
            o_sigact[i].old_handler = old_act.sa_handler;
            act.sa_handler = temp_sig_handler;
        }

        sigaction(i, &act, NULL);
        continue;
    }
    sig_taken = 1;
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

