/* Copyright (C) 2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#include "android/avd/hw-config.h"
#include "android/utils/ini.h"
#include "android/utils/system.h"
#include <string.h>
#include <stdlib.h>


/* the global variable containing the hardware config for this device */
AndroidHwConfig   android_hw[1];

static int
stringToBoolean( const char* value )
{
    if (!strcmp(value,"1")    ||
        !strcmp(value,"yes")  ||
        !strcmp(value,"YES")  ||
        !strcmp(value,"true") ||
        !strcmp(value,"TRUE"))
    {
        return 1;
    }
    else
        return 0;
}

static int64_t
diskSizeToInt64( const char* diskSize )
{
    char*   end;
    int64_t value;

    value = strtoll(diskSize, &end, 10);
    if (*end == 'k' || *end == 'K')
        value *= 1024ULL;
    else if (*end == 'm' || *end == 'M')
        value *= 1024*1024ULL;
    else if (*end == 'g' || *end == 'G')
        value *= 1024*1024*1024ULL;

    return value;
}


void
androidHwConfig_init( AndroidHwConfig*  config,
                      int               apiLevel )
{
#define   HWCFG_BOOL(n,s,d,a,t)       config->n = stringToBoolean(d);
#define   HWCFG_INT(n,s,d,a,t)        config->n = d;
#define   HWCFG_STRING(n,s,d,a,t)     config->n = ASTRDUP(d);
#define   HWCFG_DOUBLE(n,s,d,a,t)     config->n = d;
#define   HWCFG_DISKSIZE(n,s,d,a,t)   config->n = diskSizeToInt64(d);

#include "android/avd/hw-config-defs.h"

    /* Special case for hw.keyboard.lid, we need to set the
     * default to FALSE for apiLevel >= 12. This allows platform builds
     * to get correct orientation emulation even if they don't bring
     * a custom hardware.ini
     */
    if (apiLevel >= 12) {
        config->hw_keyboard_lid = 0;
    }
}

int
androidHwConfig_read( AndroidHwConfig*  config,
                      IniFile*          ini )
{
    if (ini == NULL)
        return -1;

    /* use the magic of macros to implement the hardware configuration loaded */

#define   HWCFG_BOOL(n,s,d,a,t)       if (iniFile_getValue(ini, s)) { config->n = iniFile_getBoolean(ini, s, d); }
#define   HWCFG_INT(n,s,d,a,t)        if (iniFile_getValue(ini, s)) { config->n = iniFile_getInteger(ini, s, d); }
#define   HWCFG_STRING(n,s,d,a,t)     if (iniFile_getValue(ini, s)) { AFREE(config->n); config->n = iniFile_getString(ini, s, d); }
#define   HWCFG_DOUBLE(n,s,d,a,t)     if (iniFile_getValue(ini, s)) { config->n = iniFile_getDouble(ini, s, d); }
#define   HWCFG_DISKSIZE(n,s,d,a,t)   if (iniFile_getValue(ini, s)) { config->n = iniFile_getDiskSize(ini, s, d); }

#include "android/avd/hw-config-defs.h"

    return 0;
}

int
androidHwConfig_write( AndroidHwConfig* config,
                       IniFile*         ini )
{
    if (ini == NULL)
        return -1;

    /* use the magic of macros to implement the hardware configuration loaded */

#define   HWCFG_BOOL(n,s,d,a,t)       iniFile_setBoolean(ini, s, config->n);
#define   HWCFG_INT(n,s,d,a,t)        iniFile_setInteger(ini, s, config->n);
#define   HWCFG_STRING(n,s,d,a,t)     iniFile_setValue(ini, s, config->n);
#define   HWCFG_DOUBLE(n,s,d,a,t)     iniFile_setDouble(ini, s, config->n);
#define   HWCFG_DISKSIZE(n,s,d,a,t)   iniFile_setDiskSize(ini, s, config->n);

#include "android/avd/hw-config-defs.h"

    return 0;
}

void
androidHwConfig_done( AndroidHwConfig* config )
{
#define   HWCFG_BOOL(n,s,d,a,t)       config->n = 0;
#define   HWCFG_INT(n,s,d,a,t)        config->n = 0;
#define   HWCFG_STRING(n,s,d,a,t)     AFREE(config->n);
#define   HWCFG_DOUBLE(n,s,d,a,t)     config->n = 0.0;
#define   HWCFG_DISKSIZE(n,s,d,a,t)   config->n = 0;

#include "android/avd/hw-config-defs.h"
}

int
androidHwConfig_isScreenNoTouch( AndroidHwConfig* config )
{
    return strcmp(config->hw_screen, "no-touch") == 0;
}

int
androidHwConfig_isScreenTouch( AndroidHwConfig* config )
{
    return strcmp(config->hw_screen, "touch") == 0;
}

int
androidHwConfig_isScreenMultiTouch( AndroidHwConfig* config )
{
    return strcmp(config->hw_screen, "multi-touch") == 0;
}
