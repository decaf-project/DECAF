/* x86-specific configuration */
#include "android/config/config.h"
#define TARGET_I386 1
/* For now, KVM is only supported on Linux hosts */
#ifdef CONFIG_LINUX
#define CONFIG_KVM  1
#endif
