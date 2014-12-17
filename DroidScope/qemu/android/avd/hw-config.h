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
#ifndef _ANDROID_AVD_HW_CONFIG_H
#define _ANDROID_AVD_HW_CONFIG_H

#include <stdint.h>
#include "android/utils/ini.h"

typedef char      hw_bool_t;
typedef int       hw_int_t;
typedef int64_t   hw_disksize_t;
typedef char*     hw_string_t;
typedef double    hw_double_t;

/* these macros are used to define the fields of AndroidHwConfig
 * declared below
 */
#define   HWCFG_BOOL(n,s,d,a,t)       hw_bool_t      n;
#define   HWCFG_INT(n,s,d,a,t)        hw_int_t       n;
#define   HWCFG_STRING(n,s,d,a,t)     hw_string_t    n;
#define   HWCFG_DOUBLE(n,s,d,a,t)     hw_double_t    n;
#define   HWCFG_DISKSIZE(n,s,d,a,t)   hw_disksize_t  n;

typedef struct {
#include "android/avd/hw-config-defs.h"
} AndroidHwConfig;

/* Set all default values, based on the target API level */
void androidHwConfig_init( AndroidHwConfig*  hwConfig,
                           int               apiLevel );

/* reads a hardware configuration file from disk.
 * returns -1 if the file could not be read, or 0 in case of success.
 *
 * note that default values are written to hwConfig if the configuration
 * file doesn't have the corresponding hardware properties.
 */
int  androidHwConfig_read( AndroidHwConfig*  hwConfig,
                           IniFile*          configFile );

/* Write a hardware configuration to a config file object.
 * Returns 0 in case of success. Note that any value that is set to the
 * default will not bet written.
 */
int  androidHwConfig_write( AndroidHwConfig*  hwConfig,
                            IniFile*          configFile );

/* Finalize a given hardware configuration */
void androidHwConfig_done( AndroidHwConfig* config );

#endif /* _ANDROID_AVD_HW_CONFIG_H */
