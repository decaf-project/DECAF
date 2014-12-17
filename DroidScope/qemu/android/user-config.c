/* Copyright (C) 2009 The Android Open Source Project
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
#include "android/user-config.h"
#include "android/utils/bufprint.h"
#include "android/utils/debug.h"
#include "android/utils/system.h"
#include "android/utils/path.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#define  D(...)   VERBOSE_PRINT(init,__VA_ARGS__)

#if 0 /* set to 1 for more debugging */
#  define  DD(...)  D(__VA_ARGS__)
#else
#  define  DD(...)  ((void)0)
#endif

struct AUserConfig {
    ABool      changed;
    int        windowX;
    int        windowY;
    uint64_t   uuid;
    char*      iniPath;
};

/* Name of the user-config file */
#define  USER_CONFIG_FILE  "emulator-user.ini"

#define  KEY_WINDOW_X  "window.x"
#define  KEY_WINDOW_Y  "window.y"
#define  KEY_UUID      "uuid"

#define  DEFAULT_X  100
#define  DEFAULT_Y  100

/* Create a new AUserConfig object from a given AvdInfo */
AUserConfig*
auserConfig_new( AvdInfo*  info )
{
    AUserConfig*  uc;
    char          inAndroidBuild = avdInfo_inAndroidBuild(info);
    char          needUUID = 1;
    char          temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);
    char*         parentPath;
    IniFile*      ini = NULL;

    ANEW0(uc);

    /* If we are in the Android build system, store the configuration
     * in ~/.android/emulator-user.ini. otherwise, store it in the file
     * emulator-user.ini in the AVD's content directory.
     */
    if (inAndroidBuild) {
        p = bufprint_config_file(temp, end, USER_CONFIG_FILE);
    } else {
        p = bufprint(temp, end, "%s/%s", avdInfo_getContentPath(info), 
                     USER_CONFIG_FILE);
    }

    /* handle the unexpected */
    if (p >= end) {
        /* Hmmm, something is weird, let's use a temporary file instead */
        p = bufprint_temp_file(temp, end, USER_CONFIG_FILE);
        if (p >= end) {
            derror("Weird: Cannot create temporary user-config file?");
            exit(2);
        }
        dwarning("Weird: Content path too long, using temporary user-config.");
    }

    uc->iniPath = ASTRDUP(temp);
    DD("looking user-config in: %s", uc->iniPath);


    /* ensure that the parent directory exists */
    parentPath = path_parent(uc->iniPath, 1);
    if (parentPath == NULL) {
        derror("Weird: Can't find parent of user-config file: %s",
               uc->iniPath);
        exit(2);
    }

    if (!path_exists(parentPath)) {
        if (!inAndroidBuild) {
            derror("Weird: No content path for this AVD: %s", parentPath);
            exit(2);
        }
        DD("creating missing directory: %s", parentPath);
        if (path_mkdir_if_needed(parentPath, 0755) < 0) {
            derror("Using empty user-config, can't create %s: %s",
                   parentPath, strerror(errno));
            exit(2);
        }
    }

    if (path_exists(uc->iniPath)) {
        DD("reading user-config file");
        ini = iniFile_newFromFile(uc->iniPath);
        if (ini == NULL) {
            dwarning("Can't read user-config file: %s\nUsing default values",
                     uc->iniPath);
        }
    }

    if (ini != NULL) {
        uc->windowX = iniFile_getInteger(ini, KEY_WINDOW_X, DEFAULT_X);
        DD("    found %s = %d", KEY_WINDOW_X, uc->windowX);

        uc->windowY = iniFile_getInteger(ini, KEY_WINDOW_Y, DEFAULT_Y);
        DD("    found %s = %d", KEY_WINDOW_Y, uc->windowY);

        if (iniFile_getValue(ini, KEY_UUID) != NULL) {
            uc->uuid = (uint64_t) iniFile_getInt64(ini, KEY_UUID, 0LL);
            needUUID = 0;
            DD("    found %s = %lld", KEY_UUID, uc->uuid);
        }

        iniFile_free(ini);
    }
    else {
        uc->windowX = DEFAULT_X;
        uc->windowY = DEFAULT_Y;
        uc->changed = 1;
    }

    /* Generate a 64-bit UUID if necessary. We simply take the
     * current time, which avoids any privacy-related value.
     */
    if (needUUID) {
        struct timeval  tm;

        gettimeofday( &tm, NULL );
        uc->uuid    = (uint64_t)tm.tv_sec*1000 + tm.tv_usec/1000;
        uc->changed = 1;
        DD("    Generated UUID = %lld", uc->uuid);
    }

    return uc;
}


uint64_t
auserConfig_getUUID( AUserConfig*  uconfig )
{
    return uconfig->uuid;
}

void
auserConfig_getWindowPos( AUserConfig*  uconfig, int  *pX, int  *pY )
{
    *pX = uconfig->windowX;
    *pY = uconfig->windowY;
}


void
auserConfig_setWindowPos( AUserConfig*  uconfig, int  x, int  y )
{
    if (x != uconfig->windowX || y != uconfig->windowY) {
        uconfig->windowX = x;
        uconfig->windowY = y;
        uconfig->changed = 1;
    }
}

/* Save the user configuration back to the content directory.
 * Should be used in an atexit() handler */
void
auserConfig_save( AUserConfig*  uconfig )
{
    IniFile*   ini;
    char       temp[256];

    if (uconfig->changed == 0) {
        D("User-config was not changed.");
        return;
    }

    bufprint(temp, temp+sizeof(temp),
             "%s = %d\n"
             "%s = %d\n"
             "%s = %lld\n",
             KEY_WINDOW_X, uconfig->windowX,
             KEY_WINDOW_Y, uconfig->windowY,
             KEY_UUID,     uconfig->uuid );

    DD("Generated user-config file:\n%s", temp);

    ini = iniFile_newFromMemory(temp, uconfig->iniPath);
    if (ini == NULL) {
        D("Weird: can't create user-config iniFile?");
        return;
    }
    if (iniFile_saveToFile(ini, uconfig->iniPath) < 0) {
        dwarning("could not save user configuration: %s: %s",
                 uconfig->iniPath, strerror(errno));
    } else {
        D("User configuration saved to %s", uconfig->iniPath);
    }
    iniFile_free(ini);
}
