#define SYSCALLTABLE_LINUX_2_6_LEN 338

static const char* sysCallTable[SYSCALLTABLE_LINUX_2_6_LEN] = {
  "sys_restart_syscall",
      /* 0 - old "setup()" system call, used for restarting */
  "sys_exit",
  "ptregs_fork",
  "sys_read",
  "sys_write",
  "sys_open",
         /* 5 */
  "sys_close",
  "sys_waitpid",
  "sys_creat",
  "sys_link",
  "sys_unlink",
       /* 10 */
  "ptregs_execve",
  "sys_chdir",
  "sys_time",
  "sys_mknod",
  "sys_chmod",
        /* 15 */
  "sys_lchown16",
  "sys_ni_syscall",
   /* old break syscall holder */
  "sys_stat",
  "sys_lseek",
  "sys_getpid",
       /* 20 */
  "sys_mount",
  "sys_oldumount",
  "sys_setuid16",
  "sys_getuid16",
  "sys_stime",
        /* 25 */
  "sys_ptrace",
  "sys_alarm",
  "sys_fstat",
  "sys_pause",
  "sys_utime",
        /* 30 */
  "sys_ni_syscall",
   /* old stty syscall holder */
  "sys_ni_syscall",
   /* old gtty syscall holder */
  "sys_access",
  "sys_nice",
  "sys_ni_syscall",
   /* 35 - old ftime syscall holder */
  "sys_sync",
  "sys_kill",
  "sys_rename",
  "sys_mkdir",
  "sys_rmdir",
        /* 40 */
  "sys_dup",
  "sys_pipe",
  "sys_times",
  "sys_ni_syscall",
   /* old prof syscall holder */
  "sys_brk",
          /* 45 */
  "sys_setgid16",
  "sys_getgid16",
  "sys_signal",
  "sys_geteuid16",
  "sys_getegid16",
    /* 50 */
  "sys_acct",
  "sys_umount",
       /* recycled never used phys() */
  "sys_ni_syscall",
   /* old lock syscall holder */
  "sys_ioctl",
  "sys_fcntl",
        /* 55 */
  "sys_ni_syscall",
   /* old mpx syscall holder */
  "sys_setpgid",
  "sys_ni_syscall",
   /* old ulimit syscall holder */
  "sys_olduname",
  "sys_umask",
        /* 60 */
  "sys_chroot",
  "sys_ustat",
  "sys_dup2",
  "sys_getppid",
  "sys_getpgrp",
      /* 65 */
  "sys_setsid",
  "sys_sigaction",
  "sys_sgetmask",
  "sys_ssetmask",
  "sys_setreuid16",
   /* 70 */
  "sys_setregid16",
  "sys_sigsuspend",
  "sys_sigpending",
  "sys_sethostname",
  "sys_setrlimit",
    /* 75 */
  "sys_old_getrlimit",
  "sys_getrusage",
  "sys_gettimeofday",
  "sys_settimeofday",
  "sys_getgroups16",
  /* 80 */
  "sys_setgroups16",
  "old_select",
  "sys_symlink",
  "sys_lstat",
  "sys_readlink",
     /* 85 */
  "sys_uselib",
  "sys_swapon",
  "sys_reboot",
  "sys_old_readdir",
  "old_mmap",
         /* 90 */
  "sys_munmap",
  "sys_truncate",
  "sys_ftruncate",
  "sys_fchmod",
  "sys_fchown16",
     /* 95 */
  "sys_getpriority",
  "sys_setpriority",
  "sys_ni_syscall",
   /* old profil syscall holder */
  "sys_statfs",
  "sys_fstatfs",
      /* 100 */
  "sys_ioperm",
  "sys_socketcall",
  "sys_syslog",
  "sys_setitimer",
  "sys_getitimer",
    /* 105 */
  "sys_newstat",
  "sys_newlstat",
  "sys_newfstat",
  "sys_uname",
  "ptregs_iopl",
      /* 110 */
  "sys_vhangup",
  "sys_ni_syscall",
   /* old "idle" system call */
  "ptregs_vm86old",
  "sys_wait4",
  "sys_swapoff",
      /* 115 */
  "sys_sysinfo",
  "sys_ipc",
  "sys_fsync",
  "ptregs_sigreturn",
  "ptregs_clone",
     /* 120 */
  "sys_setdomainname",
  "sys_newuname",
  "sys_modify_ldt",
  "sys_adjtimex",
  "sys_mprotect",
     /* 125 */
  "sys_sigprocmask",
  "sys_ni_syscall",
   /* old "create_module" */
  "sys_init_module",
  "sys_delete_module",
  "sys_ni_syscall",
   /* 130: old "get_kernel_syms" */
  "sys_quotactl",
  "sys_getpgid",
  "sys_fchdir",
  "sys_bdflush",
  "sys_sysfs",
        /* 135 */
  "sys_personality",
  "sys_ni_syscall",
   /* reserved for afs_syscall */
  "sys_setfsuid16",
  "sys_setfsgid16",
  "sys_llseek",
       /* 140 */
  "sys_getdents",
  "sys_select",
  "sys_flock",
  "sys_msync",
  "sys_readv",
        /* 145 */
  "sys_writev",
  "sys_getsid",
  "sys_fdatasync",
  "sys_sysctl",
  "sys_mlock",
        /* 150 */
  "sys_munlock",
  "sys_mlockall",
  "sys_munlockall",
  "sys_sched_setparam",
  "sys_sched_getparam",
  /* 155 */
  "sys_sched_setscheduler",
  "sys_sched_getscheduler",
  "sys_sched_yield",
  "sys_sched_get_priority_max",
  "sys_sched_get_priority_min",
 /* 160 */
  "sys_sched_rr_get_interval",
  "sys_nanosleep",
  "sys_mremap",
  "sys_setresuid16",
  "sys_getresuid16",
  /* 165 */
  "ptregs_vm86",
  "sys_ni_syscall",
   /* Old sys_query_module */
  "sys_poll",
  "sys_nfsservctl",
  "sys_setresgid16",
  /* 170 */
  "sys_getresgid16",
  "sys_prctl",
  "ptregs_rt_sigreturn",
  "sys_rt_sigaction",
  "sys_rt_sigprocmask",
       /* 175 */
  "sys_rt_sigpending",
  "sys_rt_sigtimedwait",
  "sys_rt_sigqueueinfo",
  "sys_rt_sigsuspend",
  "sys_pread64",
      /* 180 */
  "sys_pwrite64",
  "sys_chown16",
  "sys_getcwd",
  "sys_capget",
  "sys_capset",
       /* 185 */
  "ptregs_sigaltstack",
  "sys_sendfile",
  "sys_ni_syscall",
   /* reserved for streams1 */
  "sys_ni_syscall",
   /* reserved for streams2 */
  "ptregs_vfork",
     /* 190 */
  "sys_getrlimit",
  "sys_mmap2",
  "sys_truncate64",
  "sys_ftruncate64",
  "sys_stat64",
       /* 195 */
  "sys_lstat64",
  "sys_fstat64",
  "sys_lchown",
  "sys_getuid",
  "sys_getgid",
       /* 200 */
  "sys_geteuid",
  "sys_getegid",
  "sys_setreuid",
  "sys_setregid",
  "sys_getgroups",
    /* 205 */
  "sys_setgroups",
  "sys_fchown",
  "sys_setresuid",
  "sys_getresuid",
  "sys_setresgid",
    /* 210 */
  "sys_getresgid",
  "sys_chown",
  "sys_setuid",
  "sys_setgid",
  "sys_setfsuid",
     /* 215 */
  "sys_setfsgid",
  "sys_pivot_root",
  "sys_mincore",
  "sys_madvise",
  "sys_getdents64",
   /* 220 */
  "sys_fcntl64",
  "sys_ni_syscall",
   /* reserved for TUX */
  "sys_ni_syscall",
  "sys_gettid",
  "sys_readahead",
    /* 225 */
  "sys_setxattr",
  "sys_lsetxattr",
  "sys_fsetxattr",
  "sys_getxattr",
  "sys_lgetxattr",
    /* 230 */
  "sys_fgetxattr",
  "sys_listxattr",
  "sys_llistxattr",
  "sys_flistxattr",
  "sys_removexattr",
  /* 235 */
  "sys_lremovexattr",
  "sys_fremovexattr",
  "sys_tkill",
  "sys_sendfile64",
  "sys_futex",
        /* 240 */
  "sys_sched_setaffinity",
  "sys_sched_getaffinity",
  "sys_set_thread_area",
  "sys_get_thread_area",
  "sys_io_setup",
     /* 245 */
  "sys_io_destroy",
  "sys_io_getevents",
  "sys_io_submit",
  "sys_io_cancel",
  "sys_fadvise64",
    /* 250 */
  "sys_ni_syscall",
  "sys_exit_group",
  "sys_lookup_dcookie",
  "sys_epoll_create",
  "sys_epoll_ctl",
    /* 255 */
  "sys_epoll_wait",
  "sys_remap_file_pages",
  "sys_set_tid_address",
  "sys_timer_create",
  "sys_timer_settime",
        /* 260 */
  "sys_timer_gettime",
  "sys_timer_getoverrun",
  "sys_timer_delete",
  "sys_clock_settime",
  "sys_clock_gettime",
        /* 265 */
  "sys_clock_getres",
  "sys_clock_nanosleep",
  "sys_statfs64",
  "sys_fstatfs64",
  "sys_tgkill",
       /* 270 */
  "sys_utimes",
  "sys_fadvise64_64",
  "sys_ni_syscall",
   /* sys_vserver */
  "sys_mbind",
  "sys_get_mempolicy",
  "sys_set_mempolicy",
  "sys_mq_open",
  "sys_mq_unlink",
  "sys_mq_timedsend",
  "sys_mq_timedreceive",
      /* 280 */
  "sys_mq_notify",
  "sys_mq_getsetattr",
  "sys_kexec_load",
  "sys_waitid",
  "sys_ni_syscall",
           /* 285 */ /* available */
  "sys_add_key",
  "sys_request_key",
  "sys_keyctl",
  "sys_ioprio_set",
  "sys_ioprio_get",
           /* 290 */
  "sys_inotify_init",
  "sys_inotify_add_watch",
  "sys_inotify_rm_watch",
  "sys_migrate_pages",
  "sys_openat",
               /* 295 */
  "sys_mkdirat",
  "sys_mknodat",
  "sys_fchownat",
  "sys_futimesat",
  "sys_fstatat64",
            /* 300 */
  "sys_unlinkat",
  "sys_renameat",
  "sys_linkat",
  "sys_symlinkat",
  "sys_readlinkat",
           /* 305 */
  "sys_fchmodat",
  "sys_faccessat",
  "sys_pselect6",
  "sys_ppoll",
  "sys_unshare",
              /* 310 */
  "sys_set_robust_list",
  "sys_get_robust_list",
  "sys_splice",
  "sys_sync_file_range",
  "sys_tee",
                  /* 315 */
  "sys_vmsplice",
  "sys_move_pages",
  "sys_getcpu",
  "sys_epoll_pwait",
  "sys_utimensat",
            /* 320 */
  "sys_signalfd",
  "sys_timerfd_create",
  "sys_eventfd",
  "sys_fallocate",
  "sys_timerfd_settime",
      /* 325 */
  "sys_timerfd_gettime",
  "sys_signalfd4",
  "sys_eventfd2",
  "sys_epoll_create1",
  "sys_dup3",
                 /* 330 */
  "sys_pipe2",
  "sys_inotify_init1",
  "sys_preadv",
  "sys_pwritev",
  "sys_rt_tgsigqueueinfo",
    /* 335 */
  "sys_perf_counter_open",
  "sys_recvmmsg"
};

