/* Copyright (C) 2012 The Android Open Source Project
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

#include "qemu-common.h"
#include "android/globals.h"
#include "android/snaphost-android.h"
#include "android/utils/debug.h"

#define  E(...)    derror(__VA_ARGS__)
#define  W(...)    dwarning(__VA_ARGS__)
#define  D(...)    VERBOSE_PRINT(init,__VA_ARGS__)

/* Compares two instance of an ini file.
 * This routine compares all entries (key,value pairs) found in one ini file
 * against entries in another file. The files are considered to be equal if:
 * 1. Number of entries in each file is equal.
 * 2. Each entry in one file has a corresponded entry in another file, and their
 *    values are equal.
 * Param:
 *  current - Ini file containing the current configuration.
 *  saved - Ini file containing a previously saved configuration.
 * Return:
 *  0 if files are equal, or 1 if they are not equal, or -1 if an error has
 * occurred.
 */
static int
_cmp_hw_config(IniFile* current, IniFile* saved)
{
    int n, ret = 0;
    const int num_pairs = iniFile_getPairCount(current);

    /* Check 1: must contain same number of entries. */
    if (num_pairs != iniFile_getPairCount(saved)) {
        D("Different numbers of entries in the HW config files. Current contans %d, while saved contains %d entries.",
          num_pairs, iniFile_getPairCount(saved));
        return -1;
    }

    /* Iterate through the entries in the current file, comparing them to entries
     * in the saved file. */
    for (n = 0; n < num_pairs && ret == 0; n++) {
        char* key, *value1, *value2;

        if (iniFile_getEntry(current, n, &key, &value1)) {
            D("Unable to obtain entry %d from the current HW config file", n);
            return -1;
        }

        value2 = iniFile_getString(saved, key, "");
        if (value2 == NULL) {
            D("Saved HW config file is missing entry ('%s', '%s') found in the current HW config.",
              key, value1);
            free(key);
            free(value1);
            return 1;
        }

        ret = strcmp(value1, value2);
        if (ret) {
            D("HW config value mismatch for a key '%s': current is '%s' while saved was '%s'",
              key, value1, value2);
        }

        free(value2);
        free(value1);
        free(key);
    }

    return ret ? 1 : 0;
}

/* Builds path to the HW config backup file that is used to store HW config
 * settings used for that snapshot. The backup path is a concatenation of the
 * snapshot storage file path, snapshot name, and an 'ini' extension. This
 * way we can track HW configuration for different snapshot names store in
 * different storage files.
 * Param:
 *  name - Name of the snapshot inside the snapshot storage file.
 * Return:
 *  Path to the HW config backup file on success, or NULL on an error.
 */
static char*
_build_hwcfg_path(const char* name)
{
    const int path_len = strlen(android_hw->disk_snapStorage_path) +
                         strlen(name) + 6;
    char* bkp_path = malloc(path_len);
    if (bkp_path == NULL) {
        E("Unable to allocate %d bytes for HW config path!", path_len);
        return NULL;
    }

    snprintf(bkp_path, path_len, "%s.%s.ini",
             android_hw->disk_snapStorage_path, name);

    return bkp_path;
}

int
snaphost_match_configs(IniFile* hw_ini, const char* name)
{
    /* Make sure that snapshot storage path is valid. */
    if (android_hw->disk_snapStorage_path == NULL ||
        *android_hw->disk_snapStorage_path == '\0') {
        return 1;
    }

    /* Build path to the HW config for the loading VM. */
    char* bkp_path = _build_hwcfg_path(name);
    if (bkp_path == NULL) {
        return 0;
    }

    /* Load HW config from the previous emulator launch. */
    IniFile* hwcfg_bkp = iniFile_newFromFile(bkp_path);

    if (hwcfg_bkp != NULL) {
        if (_cmp_hw_config(hw_ini, hwcfg_bkp)) {
            E("Unable to load VM from snapshot. The snapshot has been saved for a different hardware configuration.");
            free(bkp_path);
            return 0;
        }
        iniFile_free(hwcfg_bkp);
    } else {
        /* It could be that a snapshot file has been copied from another
         * location without copying the backup file, or snapshot file has not
         * been created yet. In either case we can't do much checking here,
         * so, lets be hopefull that user knows what (s)he is doing. */
        D("Missing HW config backup file '%s'", bkp_path);
    }

    free(bkp_path);

    return 1;
}

void
snaphost_save_config(const char* name)
{
    /* Make sure that snapshot storage path is valid. */
    if (android_hw->disk_snapStorage_path == NULL ||
        *android_hw->disk_snapStorage_path == '\0') {
        return;
    }

    /* Build path to the HW config for the saving VM. */
    char* bkp_path = _build_hwcfg_path(name);
    if (bkp_path == NULL) {
        return;
    }

    /* Create HW config backup file from the current HW config settings. */
    IniFile* hwcfg_bkp = iniFile_newFromMemory("", bkp_path);
    if (hwcfg_bkp == NULL) {
        W("Unable to create backup HW config file '%s'. Error: %s",
          bkp_path, strerror(errno));
        return;
    }
    androidHwConfig_write(android_hw, hwcfg_bkp);

    /* Invalidate data partition initialization path in the backup copy of HW
     * config. The reason we need to do this is that we want the system loading
     * from the snapshot to be in sync with the data partition the snapshot was
     * saved for. For that we must disalow overwritting it on snapshot load. In
     * other words, we should allow snapshot loading only on condition
     * that disk.dataPartition.initPath is empty. */
    iniFile_setValue(hwcfg_bkp, "disk.dataPartition.initPath", "");

    /* Save backup file. */
    if (!iniFile_saveToFileClean(hwcfg_bkp, bkp_path)) {
        D("HW config has been backed up to '%s'", bkp_path);
    } else {
        /* Treat this as a "soft" error. Yes, we couldn't save the backup, but
         * this should not cancel snapshot saving. */
        W("Unable to save HW config file '%s'. Error: %s", bkp_path, strerror(errno));
    }
    iniFile_free(hwcfg_bkp);
    free(bkp_path);
}
