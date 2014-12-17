/* This file is included by target-specific files under
 * android/config/target-$ARCH/config.h, but contains all config
 * definitions that are independent of the target CPU.
 *
 * Do not include directly.
 */
#include "config-host.h"

#define TARGET_PHYS_ADDR_BITS  32
#define CONFIG_TRACE 1
#define CONFIG_NAND 1
#define CONFIG_SHAPER 1
#define CONFIG_SOFTMMU 1
#define CONFIG_SDL 1
#ifndef _WIN32
#define CONFIG_NAND_LIMITS 1
#endif
#define CONFIG_MEMCHECK 1
