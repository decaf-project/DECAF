#ifndef _KVM_GS_RESTORE_H
#define _KVM_GS_RESTORE_H

#ifdef CONFIG_KVM_GS_RESTORE
int no_gs_ioctl(int fd, int type, void *arg);
int gs_base_post_run(void);
int gs_base_pre_run(void);
extern int gs_need_restore;
#define KVM_GS_RESTORE_NO 0x2
#endif

#endif
