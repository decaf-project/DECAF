/* x86-specific configuration */
#include "android/config/config.h"
#define TARGET_I386 1
/* For now, KVM is only supported on Linux hosts */
#ifdef CONFIG_LINUX
#define CONFIG_KVM  1
#endif

/*
 * HAX is supported in darwin and windows host
 */
#ifdef CONFIG_DARWIN
#define CONFIG_HAX 1
#endif

#ifdef CONFIG_WIN32
#define CONFIG_HAX 1
#endif
