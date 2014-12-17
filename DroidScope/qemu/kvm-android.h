#ifndef KVM_ANDROID_H
#define KVM_ANDROID_H

/* Returns 1 if we can use /dev/kvm on this machine */
extern int kvm_check_allowed(void);

#endif /* KVM_ANDROID_H */
