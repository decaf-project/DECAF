#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>
#include "android/utils/debug.h"

#define D(...) VERBOSE_PRINT(init,__VA_ARGS__)

/* A simple routine used to check that we can run the program under KVM.
 * We simply want to ensure that the kvm driver is loaded and that the
 * corresponding device file is accessible by the user.
 */

#ifndef __linux__
#error "This file should only be compiled under linux"
#endif

int
kvm_check_allowed(void)
{
    /* Is there a /dev/kvm device file here? */
    if (access("/dev/kvm",F_OK)) {
        /* no need to print a warning here */
        D("No kvm device file detected");
        return 0;
    }

    /* Can we access it? */
    if (access("/dev/kvm",R_OK)) {
        D("KVM device file is not readable for this user.");
        return 0;
    }

    D("KVM mode auto-enabled!");
    return 1;
}

