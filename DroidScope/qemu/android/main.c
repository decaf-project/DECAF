/* Copyright (C) 2006-2008 The Android Open Source Project
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

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#ifdef _WIN32
#include <process.h>
#endif

#include "sockets.h"

#include "android/android.h"
#include "qemu-common.h"
#include "sysemu.h"
#include "console.h"
#include "user-events.h"

#include <SDL.h>
#include <SDL_syswm.h>

#include "math.h"

#include "android/charmap.h"
#include "android/utils/debug.h"
#include "android/config.h"
#include "android/config/config.h"

#include "android/user-config.h"
#include "android/utils/bufprint.h"
#include "android/utils/filelock.h"
#include "android/utils/lineinput.h"
#include "android/utils/path.h"
#include "android/utils/tempfile.h"

#include "android/main-common.h"
#include "android/help.h"
#include "hw/goldfish_nand.h"

#include "android/globals.h"

#include "android/qemulator.h"
#include "android/display.h"

#include "android/snapshot.h"

#include "android/framebuffer.h"
#include "iolooper.h"

AndroidRotation  android_framebuffer_rotation;

#define  STRINGIFY(x)   _STRINGIFY(x)
#define  _STRINGIFY(x)  #x

#ifdef ANDROID_SDK_TOOLS_REVISION
#  define  VERSION_STRING  STRINGIFY(ANDROID_SDK_TOOLS_REVISION)".0"
#else
#  define  VERSION_STRING  "standalone"
#endif

#define  D(...)  do {  if (VERBOSE_CHECK(init)) dprint(__VA_ARGS__); } while (0)

extern int  control_console_start( int  port );  /* in control.c */

extern int qemu_milli_needed;

/* the default device DPI if none is specified by the skin
 */
#define  DEFAULT_DEVICE_DPI  165

#ifdef CONFIG_TRACE
extern void  start_tracing(void);
extern void  stop_tracing(void);
#endif

unsigned long   android_verbose;

int qemu_main(int argc, char **argv);

/* this function dumps the QEMU help */
extern void  help( void );
extern void  emulator_help( void );

#define  VERBOSE_OPT(str,var)   { str, &var }

#define  _VERBOSE_TAG(x,y)   { #x, VERBOSE_##x, y },
static const struct { const char*  name; int  flag; const char*  text; }
verbose_options[] = {
    VERBOSE_TAG_LIST
    { 0, 0, 0 }
};

void emulator_help( void )
{
    STRALLOC_DEFINE(out);
    android_help_main(out);
    printf( "%.*s", out->n, out->s );
    stralloc_reset(out);
    exit(1);
}

/* TODO: Put in shared source file */
static char*
_getFullFilePath( const char* rootPath, const char* fileName )
{
    if (path_is_absolute(fileName)) {
        return ASTRDUP(fileName);
    } else {
        char temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);

        p = bufprint(temp, end, "%s/%s", rootPath, fileName);
        if (p >= end) {
            return NULL;
        }
        return ASTRDUP(temp);
    }
}

static uint64_t
_adjustPartitionSize( const char*  description,
                      uint64_t     imageBytes,
                      uint64_t     defaultBytes,
                      int          inAndroidBuild )
{
    char      temp[64];
    unsigned  imageMB;
    unsigned  defaultMB;

    if (imageBytes <= defaultBytes)
        return defaultBytes;

    imageMB   = convertBytesToMB(imageBytes);
    defaultMB = convertBytesToMB(defaultBytes);

    if (imageMB > defaultMB) {
        snprintf(temp, sizeof temp, "(%d MB > %d MB)", imageMB, defaultMB);
    } else {
        snprintf(temp, sizeof temp, "(%" PRIu64 "  bytes > %" PRIu64 " bytes)", imageBytes, defaultBytes);
    }

    if (inAndroidBuild) {
        dwarning("%s partition size adjusted to match image file %s\n", description, temp);
    }

    return convertMBToBytes(imageMB);
}

int main(int argc, char **argv)
{
    char   tmp[MAX_PATH];
    char*  tmpend = tmp + sizeof(tmp);
    char*  args[128];
    int    n;
    char*  opt;
    /* The emulator always uses the first serial port for kernel messages
     * and the second one for qemud. So start at the third if we need one
     * for logcat or 'shell'
     */
    int    serial = 2;
    int    shell_serial = 0;

    int    forceArmv7 = 0;

    AndroidHwConfig*  hw;
    AvdInfo*          avd;
    AConfig*          skinConfig;
    char*             skinPath;
    int               inAndroidBuild;
    uint64_t          defaultPartitionSize = convertMBToBytes(200);

    AndroidOptions  opts[1];
    /* net.shared_net_ip boot property value. */
    char boot_prop_ip[64];
    boot_prop_ip[0] = '\0';

    args[0] = argv[0];

    if ( android_parse_options( &argc, &argv, opts ) < 0 ) {
        exit(1);
    }

#ifdef _WIN32
    socket_init();
#endif

    handle_ui_options(opts);

    while (argc-- > 1) {
        opt = (++argv)[0];

        if(!strcmp(opt, "-qemu")) {
            argc--;
            argv++;
            break;
        }

        if (!strcmp(opt, "-help")) {
            emulator_help();
        }

        if (!strncmp(opt, "-help-",6)) {
            STRALLOC_DEFINE(out);
            opt += 6;

            if (!strcmp(opt, "all")) {
                android_help_all(out);
            }
            else if (android_help_for_option(opt, out) == 0) {
                /* ok */
            }
            else if (android_help_for_topic(opt, out) == 0) {
                /* ok */
            }
            if (out->n > 0) {
                printf("\n%.*s", out->n, out->s);
                exit(0);
            }

            fprintf(stderr, "unknown option: -help-%s\n", opt);
            fprintf(stderr, "please use -help for a list of valid topics\n");
            exit(1);
        }

        if (opt[0] == '-') {
            fprintf(stderr, "unknown option: %s\n", opt);
            fprintf(stderr, "please use -help for a list of valid options\n");
            exit(1);
        }

        fprintf(stderr, "invalid command-line parameter: %s.\n", opt);
        fprintf(stderr, "Hint: use '@foo' to launch a virtual device named 'foo'.\n");
        fprintf(stderr, "please use -help for more information\n");
        exit(1);
    }

    if (opts->version) {
        printf("Android emulator version %s\n"
               "Copyright (C) 2006-2011 The Android Open Source Project and many others.\n"
               "This program is a derivative of the QEMU CPU emulator (www.qemu.org).\n\n",
#if defined ANDROID_BUILD_ID
               VERSION_STRING " (build_id " STRINGIFY(ANDROID_BUILD_ID) ")" );
#else
               VERSION_STRING);
#endif
        printf("  This software is licensed under the terms of the GNU General Public\n"
               "  License version 2, as published by the Free Software Foundation, and\n"
               "  may be copied, distributed, and modified under those terms.\n\n"
               "  This program is distributed in the hope that it will be useful,\n"
               "  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
               "  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
               "  GNU General Public License for more details.\n\n");

        exit(0);
    }

    if (opts->snapshot_list) {
        if (opts->snapstorage == NULL) {
            /* Need to find the default snapstorage */
            avd = createAVD(opts, &inAndroidBuild);
            opts->snapstorage = avdInfo_getSnapStoragePath(avd);
            if (opts->snapstorage != NULL) {
                D("autoconfig: -snapstorage %s", opts->snapstorage);
            } else {
                if (inAndroidBuild) {
                    derror("You must use the -snapstorage <file> option to specify a snapshot storage file!\n");
                } else {
                    derror("This AVD doesn't have snapshotting enabled!\n");
                }
                exit(1);
            }
        }
        snapshot_print_and_exit(opts->snapstorage);
    }

    sanitizeOptions(opts);

    /* Initialization of UI started with -attach-core should work differently
     * than initialization of UI that starts the core. In particular....
     */

    /* -charmap is incompatible with -attach-core, because particular
     * charmap gets set up in the running core. */
    if (android_charmap_setup(opts->charmap)) {
        exit(1);
    }

    /* Parses options and builds an appropriate AVD. */
    avd = android_avdInfo = createAVD(opts, &inAndroidBuild);

    /* get the skin from the virtual device configuration */
    if (opts->skindir != NULL) {
        if (opts->skin == NULL) {
            /* NOTE: Normally handled by sanitizeOptions(), just be safe */
            derror("The -skindir <path> option requires a -skin <name> option");
            exit(2);
        }
    } else {
        char* skinName;
        char* skinDir;

        avdInfo_getSkinInfo(avd, &skinName, &skinDir);

        if (opts->skin == NULL) {
            opts->skin = skinName;
            D("autoconfig: -skin %s", opts->skin);
        } else {
            AFREE(skinName);
        }

        opts->skindir = skinDir;
        D("autoconfig: -skindir %s", opts->skindir);

        /* update the avd hw config from this new skin */
        avdInfo_getSkinHardwareIni(avd, opts->skin, opts->skindir);
    }

    if (opts->dynamic_skin == 0) {
        opts->dynamic_skin = avdInfo_shouldUseDynamicSkin(avd);
    }

    /* Read hardware configuration */
    hw = android_hw;
    if (avdInfo_initHwConfig(avd, hw) < 0) {
        derror("could not read hardware configuration ?");
        exit(1);
    }

    if (opts->keyset) {
        parse_keyset(opts->keyset, opts);
        if (!android_keyset) {
            fprintf(stderr,
                    "emulator: WARNING: could not find keyset file named '%s',"
                    " using defaults instead\n",
                    opts->keyset);
        }
    }
    if (!android_keyset) {
        parse_keyset("default", opts);
        if (!android_keyset) {
            android_keyset = skin_keyset_new_from_text( skin_keyset_get_default() );
            if (!android_keyset) {
                fprintf(stderr, "PANIC: default keyset file is corrupted !!\n" );
                fprintf(stderr, "PANIC: please update the code in android/skin/keyset.c\n" );
                exit(1);
            }
            if (!opts->keyset)
                write_default_keyset();
        }
    }

    if (opts->shared_net_id) {
        char*  end;
        long   shared_net_id = strtol(opts->shared_net_id, &end, 0);
        if (end == NULL || *end || shared_net_id < 1 || shared_net_id > 255) {
            fprintf(stderr, "option -shared-net-id must be an integer between 1 and 255\n");
            exit(1);
        }
        snprintf(boot_prop_ip, sizeof(boot_prop_ip),
                 "net.shared_net_ip=10.1.2.%ld", shared_net_id);
    }


    user_config_init();
    parse_skin_files(opts->skindir, opts->skin, opts, hw,
                     &skinConfig, &skinPath);

    if (!opts->netspeed && skin_network_speed) {
        D("skin network speed: '%s'", skin_network_speed);
        if (strcmp(skin_network_speed, NETWORK_SPEED_DEFAULT) != 0) {
            opts->netspeed = (char*)skin_network_speed;
        }
    }
    if (!opts->netdelay && skin_network_delay) {
        D("skin network delay: '%s'", skin_network_delay);
        if (strcmp(skin_network_delay, NETWORK_DELAY_DEFAULT) != 0) {
            opts->netdelay = (char*)skin_network_delay;
        }
    }

    if (opts->trace) {
        char*   tracePath = avdInfo_getTracePath(avd, opts->trace);
        int     ret;

        if (tracePath == NULL) {
            derror( "bad -trace parameter" );
            exit(1);
        }
        ret = path_mkdir_if_needed( tracePath, 0755 );
        if (ret < 0) {
            fprintf(stderr, "could not create directory '%s'\n", tmp);
            exit(2);
        }
        opts->trace = tracePath;
    }

    /* Update CPU architecture for HW configs created from build dir. */
    if (inAndroidBuild) {
#if defined(TARGET_ARM)
        free(android_hw->hw_cpu_arch);
        android_hw->hw_cpu_arch = ASTRDUP("arm");
#elif defined(TARGET_I386)
        free(android_hw->hw_cpu_arch);
        android_hw->hw_cpu_arch = ASTRDUP("x86");
#elif defined(TARGET_MIPS)
        free(android_hw->hw_cpu_arch);
        android_hw->hw_cpu_arch = ASTRDUP("mips");
#endif
    }

    n = 1;
    /* generate arguments for the underlying qemu main() */
    {
        char*  kernelFile    = opts->kernel;
        int    kernelFileLen;

        if (kernelFile == NULL) {
            kernelFile = avdInfo_getKernelPath(avd);
            if (kernelFile == NULL) {
                derror( "This AVD's configuration is missing a kernel file!!" );
                exit(2);
            }
            D("autoconfig: -kernel %s", kernelFile);
        }
        if (!path_exists(kernelFile)) {
            derror( "Invalid or missing kernel image file: %s", kernelFile );
            exit(2);
        }

        hw->kernel_path = kernelFile;

        /* If the kernel image name ends in "-armv7", then change the cpu
         * type automatically. This is a poor man's approach to configuration
         * management, but should allow us to get past building ARMv7
         * system images with dex preopt pass without introducing too many
         * changes to the emulator sources.
         *
         * XXX:
         * A 'proper' change would require adding some sort of hardware-property
         * to each AVD config file, then automatically determine its value for
         * full Android builds (depending on some environment variable), plus
         * some build system changes. I prefer not to do that for now for reasons
         * of simplicity.
         */
         kernelFileLen = strlen(kernelFile);
         if (kernelFileLen > 6 && !memcmp(kernelFile + kernelFileLen - 6, "-armv7", 6)) {
             forceArmv7 = 1;
         }
    }

    if (boot_prop_ip[0]) {
        args[n++] = "-boot-property";
        args[n++] = boot_prop_ip;
    }

    if (opts->tcpdump) {
        args[n++] = "-tcpdump";
        args[n++] = opts->tcpdump;
    }

#ifdef CONFIG_NAND_LIMITS
    if (opts->nand_limits) {
        args[n++] = "-nand-limits";
        args[n++] = opts->nand_limits;
    }
#endif

    if (opts->timezone) {
        args[n++] = "-timezone";
        args[n++] = opts->timezone;
    }

    if (opts->netspeed) {
        args[n++] = "-netspeed";
        args[n++] = opts->netspeed;
    }
    if (opts->netdelay) {
        args[n++] = "-netdelay";
        args[n++] = opts->netdelay;
    }
    if (opts->netfast) {
        args[n++] = "-netfast";
    }

    if (opts->audio) {
        args[n++] = "-audio";
        args[n++] = opts->audio;
    }

    if (opts->cpu_delay) {
        args[n++] = "-cpu-delay";
        args[n++] = opts->cpu_delay;
    }

    if (opts->dns_server) {
        args[n++] = "-dns-server";
        args[n++] = opts->dns_server;
    }

    /* opts->ramdisk is never NULL (see createAVD) here */
    if (opts->ramdisk) {
        AFREE(hw->disk_ramdisk_path);
        hw->disk_ramdisk_path = ASTRDUP(opts->ramdisk);
    }
    else if (!hw->disk_ramdisk_path[0]) {
        hw->disk_ramdisk_path = avdInfo_getRamdiskPath(avd);
        D("autoconfig: -ramdisk %s", hw->disk_ramdisk_path);
    }

    /* -partition-size is used to specify the max size of both the system
     * and data partition sizes.
     */
    if (opts->partition_size) {
        char*  end;
        long   sizeMB = strtol(opts->partition_size, &end, 0);
        long   minSizeMB = 10;
        long   maxSizeMB = LONG_MAX / ONE_MB;

        if (sizeMB < 0 || *end != 0) {
            derror( "-partition-size must be followed by a positive integer" );
            exit(1);
        }
        if (sizeMB < minSizeMB || sizeMB > maxSizeMB) {
            derror( "partition-size (%d) must be between %dMB and %dMB",
                    sizeMB, minSizeMB, maxSizeMB );
            exit(1);
        }
        defaultPartitionSize = (uint64_t) sizeMB * ONE_MB;
    }


    /** SYSTEM PARTITION **/

    if (opts->sysdir == NULL) {
        if (avdInfo_inAndroidBuild(avd)) {
            opts->sysdir = ASTRDUP(avdInfo_getContentPath(avd));
            D("autoconfig: -sysdir %s", opts->sysdir);
        }
    }

    if (opts->sysdir != NULL) {
        if (!path_exists(opts->sysdir)) {
            derror("Directory does not exist: %s", opts->sysdir);
            exit(1);
        }
    }

    {
        char*  rwImage   = NULL;
        char*  initImage = NULL;

        do {
            if (opts->system == NULL) {
                /* If -system is not used, try to find a runtime system image
                * (i.e. system-qemu.img) in the content directory.
                */
                rwImage = avdInfo_getSystemImagePath(avd);
                if (rwImage != NULL) {
                    break;
                }
                /* Otherwise, try to find the initial system image */
                initImage = avdInfo_getSystemInitImagePath(avd);
                if (initImage == NULL) {
                    derror("No initial system image for this configuration!");
                    exit(1);
                }
                break;
            }

            /* If -system <name> is used, use it to find the initial image */
            if (opts->sysdir != NULL && !path_exists(opts->system)) {
                initImage = _getFullFilePath(opts->sysdir, opts->system);
            } else {
                initImage = ASTRDUP(opts->system);
            }
            if (!path_exists(initImage)) {
                derror("System image file doesn't exist: %s", initImage);
                exit(1);
            }

        } while (0);

        if (rwImage != NULL) {
            /* Use the read/write image file directly */
            hw->disk_systemPartition_path     = rwImage;
            hw->disk_systemPartition_initPath = NULL;
            D("Using direct system image: %s", rwImage);
        } else if (initImage != NULL) {
            hw->disk_systemPartition_path = NULL;
            hw->disk_systemPartition_initPath = initImage;
            D("Using initial system image: %s", initImage);
        }

        /* Check the size of the system partition image.
        * If we have an AVD, it must be smaller than
        * the disk.systemPartition.size hardware property.
        *
        * Otherwise, we need to adjust the systemPartitionSize
        * automatically, and print a warning.
        *
        */
        const char* systemImage = hw->disk_systemPartition_path;
        uint64_t    systemBytes;

        if (systemImage == NULL)
            systemImage = hw->disk_systemPartition_initPath;

        if (path_get_size(systemImage, &systemBytes) < 0) {
            derror("Missing system image: %s", systemImage);
            exit(1);
        }

        hw->disk_systemPartition_size =
            _adjustPartitionSize("system", systemBytes, defaultPartitionSize,
                                 avdInfo_inAndroidBuild(avd));
    }

    /** DATA PARTITION **/

    if (opts->datadir) {
        if (!path_exists(opts->datadir)) {
            derror("Invalid -datadir directory: %s", opts->datadir);
        }
    }

    {
        char*  dataImage = NULL;
        char*  initImage = NULL;

        do {
            if (!opts->data) {
                dataImage = avdInfo_getDataImagePath(avd);
                if (dataImage != NULL) {
                    D("autoconfig: -data %s", dataImage);
                    break;
                }
                dataImage = avdInfo_getDefaultDataImagePath(avd);
                if (dataImage == NULL) {
                    derror("No data image path for this configuration!");
                    exit (1);
                }
                opts->wipe_data = 1;
                break;
            }

            if (opts->datadir) {
                dataImage = _getFullFilePath(opts->datadir, opts->data);
            } else {
                dataImage = ASTRDUP(opts->data);
            }
        } while (0);

        if (opts->initdata != NULL) {
            initImage = ASTRDUP(opts->initdata);
            if (!path_exists(initImage)) {
                derror("Invalid initial data image path: %s", initImage);
                exit(1);
            }
        } else {
            initImage = avdInfo_getDataInitImagePath(avd);
            D("autoconfig: -initdata %s", initImage);
        }

        hw->disk_dataPartition_path = dataImage;
        if (opts->wipe_data) {
            hw->disk_dataPartition_initPath = initImage;
        } else {
            hw->disk_dataPartition_initPath = NULL;
        }

        uint64_t     defaultBytes =
                hw->disk_dataPartition_size == 0 ?
                defaultPartitionSize :
                hw->disk_dataPartition_size;
        uint64_t     dataBytes;
        const char*  dataPath = hw->disk_dataPartition_initPath;

        if (dataPath == NULL)
            dataPath = hw->disk_dataPartition_path;

        path_get_size(dataPath, &dataBytes);

        hw->disk_dataPartition_size =
            _adjustPartitionSize("data", dataBytes, defaultBytes,
                                 avdInfo_inAndroidBuild(avd));
    }

    /** CACHE PARTITION **/

    if (opts->no_cache) {
        /* No cache partition at all */
        hw->disk_cachePartition = 0;
    }
    else if (!hw->disk_cachePartition) {
        if (opts->cache) {
            dwarning( "Emulated hardware doesn't support a cache partition. -cache option ignored!" );
            opts->cache = NULL;
        }
    }
    else
    {
        if (!opts->cache) {
            /* Find the current cache partition file */
            opts->cache = avdInfo_getCachePath(avd);
            if (opts->cache == NULL) {
                /* The file does not exists, we will force its creation
                 * if we are not in the Android build system. Otherwise,
                 * a temporary file will be used.
                 */
                if (!avdInfo_inAndroidBuild(avd)) {
                    opts->cache = avdInfo_getDefaultCachePath(avd);
                }
            }
            if (opts->cache) {
                D("autoconfig: -cache %s", opts->cache);
            }
        }

        if (opts->cache) {
            hw->disk_cachePartition_path = ASTRDUP(opts->cache);
        }
    }

    if (hw->disk_cachePartition_path && opts->cache_size) {
        /* Set cache partition size per user options. */
        char*  end;
        long   sizeMB = strtol(opts->cache_size, &end, 0);

        if (sizeMB < 0 || *end != 0) {
            derror( "-cache-size must be followed by a positive integer" );
            exit(1);
        }
        hw->disk_cachePartition_size = (uint64_t) sizeMB * ONE_MB;
    }

    /** SD CARD PARTITION */

    if (!hw->hw_sdCard) {
        /* No SD Card emulation, so -sdcard will be ignored */
        if (opts->sdcard) {
            dwarning( "Emulated hardware doesn't support SD Cards. -sdcard option ignored." );
            opts->sdcard = NULL;
        }
    } else {
        /* Auto-configure -sdcard if it is not available */
        if (!opts->sdcard) {
            do {
                /* If -datadir <path> is used, look for a sdcard.img file here */
                if (opts->datadir) {
                    bufprint(tmp, tmpend, "%s/%s", opts->datadir, "system.img");
                    if (path_exists(tmp)) {
                        opts->sdcard = strdup(tmp);
                        break;
                    }
                }

                /* Otherwise, look at the AVD's content */
                opts->sdcard = avdInfo_getSdCardPath(avd);
                if (opts->sdcard != NULL) {
                    break;
                }

                /* Nothing */
            } while (0);

            if (opts->sdcard) {
                D("autoconfig: -sdcard %s", opts->sdcard);
            }
        }
    }

    if(opts->sdcard) {
        uint64_t  size;
        if (path_get_size(opts->sdcard, &size) == 0) {
            /* see if we have an sdcard image.  get its size if it exists */
            /* due to what looks like limitations of the MMC protocol, one has
             * to use an SD Card image that is equal or larger than 9 MB
             */
            if (size < 9*1024*1024ULL) {
                fprintf(stderr, "### WARNING: SD Card files must be at least 9MB, ignoring '%s'\n", opts->sdcard);
            } else {
                hw->hw_sdCard_path = ASTRDUP(opts->sdcard);
            }
        } else {
            dwarning("no SD Card image at '%s'", opts->sdcard);
        }
    }


    /** SNAPSHOT STORAGE HANDLING */

    /* Determine snapstorage path. -no-snapstorage disables all snapshotting
     * support. This means you can't resume a snapshot at load, save it at
     * exit, or even load/save them dynamically at runtime with the console.
     */
    if (opts->no_snapstorage) {

        if (opts->snapshot) {
            dwarning("ignoring -snapshot option due to the use of -no-snapstorage");
            opts->snapshot = NULL;
        }

        if (opts->snapstorage) {
            dwarning("ignoring -snapstorage option due to the use of -no-snapstorage");
            opts->snapstorage = NULL;
        }
    }
    else
    {
        if (!opts->snapstorage && avdInfo_getSnapshotPresent(avd)) {
            opts->snapstorage = avdInfo_getSnapStoragePath(avd);
            if (opts->snapstorage != NULL) {
                D("autoconfig: -snapstorage %s", opts->snapstorage);
            }
        }

        if (opts->snapstorage && !path_exists(opts->snapstorage)) {
            D("no image at '%s', state snapshots disabled", opts->snapstorage);
            opts->snapstorage = NULL;
        }
    }

    /* If we have a valid snapshot storage path */

    if (opts->snapstorage) {

        hw->disk_snapStorage_path = ASTRDUP(opts->snapstorage);

        /* -no-snapshot is equivalent to using both -no-snapshot-load
        * and -no-snapshot-save. You can still load/save snapshots dynamically
        * from the console though.
        */
        if (opts->no_snapshot) {

            opts->no_snapshot_load = 1;
            opts->no_snapshot_save = 1;

            if (opts->snapshot) {
                dwarning("ignoring -snapshot option due to the use of -no-snapshot.");
            }
        }

        if (!opts->no_snapshot_load || !opts->no_snapshot_save) {
            if (opts->snapshot == NULL) {
                opts->snapshot = "default-boot";
                D("autoconfig: -snapshot %s", opts->snapshot);
            }
        }

        /* We still use QEMU command-line options for the following since
        * they can change from one invokation to the next and don't really
        * correspond to the hardware configuration itself.
        */
        if (!opts->no_snapshot_load) {
            args[n++] = "-loadvm";
            args[n++] = ASTRDUP(opts->snapshot);
        }

        if (!opts->no_snapshot_save) {
            args[n++] = "-savevm-on-exit";
            args[n++] = ASTRDUP(opts->snapshot);
        }

        if (opts->no_snapshot_update_time) {
            args[n++] = "-snapshot-no-time-update";
        }
    }

    if (!opts->logcat || opts->logcat[0] == 0) {
        opts->logcat = getenv("ANDROID_LOG_TAGS");
        if (opts->logcat && opts->logcat[0] == 0)
            opts->logcat = NULL;
    }

    /* we always send the kernel messages from ttyS0 to android_kmsg */
    if (opts->show_kernel) {
        args[n++] = "-show-kernel";
    }

    /* XXXX: TODO: implement -shell and -logcat through qemud instead */
    if (!opts->shell_serial) {
#ifdef _WIN32
        opts->shell_serial = "con:";
#else
        opts->shell_serial = "stdio";
#endif
    }
    else
        opts->shell = 1;

    if (opts->shell || opts->logcat) {
        args[n++] = "-serial";
        args[n++] = opts->shell_serial;
        shell_serial = serial++;
    }

    if (opts->radio) {
        args[n++] = "-radio";
        args[n++] = opts->radio;
    }

    if (opts->gps) {
        args[n++] = "-gps";
        args[n++] = opts->gps;
    }

    if (opts->memory) {
        char*  end;
        long   ramSize = strtol(opts->memory, &end, 0);
        if (ramSize < 0 || *end != 0) {
            derror( "-memory must be followed by a positive integer" );
            exit(1);
        }
        if (ramSize < 32 || ramSize > 4096) {
            derror( "physical memory size must be between 32 and 4096 MB" );
            exit(1);
        }
        hw->hw_ramSize = ramSize;
    }
    if (!opts->memory) {
        int ramSize = hw->hw_ramSize;
        if (ramSize <= 0) {
            /* Compute the default RAM size based on the size of screen.
             * This is only used when the skin doesn't provide the ram
             * size through its hardware.ini (i.e. legacy ones) or when
             * in the full Android build system.
             */
            int64_t pixels  = hw->hw_lcd_width * hw->hw_lcd_height;
            /* The following thresholds are a bit liberal, but we
             * essentially want to ensure the following mappings:
             *
             *   320x480 -> 96
             *   800x600 -> 128
             *  1024x768 -> 256
             *
             * These are just simple heuristics, they could change in
             * the future.
             */
            if (pixels <= 250000)
                ramSize = 96;
            else if (pixels <= 500000)
                ramSize = 128;
            else
                ramSize = 256;
        }
        hw->hw_ramSize = ramSize;
    }

    D("Physical RAM size: %dMB\n", hw->hw_ramSize);

    if (hw->vm_heapSize == 0) {
        /* Compute the default heap size based on the RAM size.
         * Essentially, we want to ensure the following liberal mappings:
         *
         *   96MB RAM -> 16MB heap
         *  128MB RAM -> 24MB heap
         *  256MB RAM -> 48MB heap
         */
        int  ramSize = hw->hw_ramSize;
        int  heapSize;

        if (ramSize < 100)
            heapSize = 16;
        else if (ramSize < 192)
            heapSize = 24;
        else
            heapSize = 48;

        hw->vm_heapSize = heapSize;
    }

    if (opts->trace) {
        args[n++] = "-trace";
        args[n++] = opts->trace;
        args[n++] = "-tracing";
        args[n++] = "off";
    }

    /* Pass boot properties to the core. */
    if (opts->prop != NULL) {
        ParamList*  pl = opts->prop;
        for ( ; pl != NULL; pl = pl->next ) {
            args[n++] = "-boot-property";
            args[n++] = pl->param;
        }
    }

    /* Setup the kernel init options
     */
    {
        static char  params[1024];
        char        *p = params, *end = p + sizeof(params);

        /* Don't worry about having a leading space here, this is handled
         * by the core later. */

#ifdef TARGET_I386
        p = bufprint(p, end, " androidboot.hardware=goldfish");
        p = bufprint(p, end, " clocksource=pit");
#endif

        if (opts->shell || opts->logcat) {
            p = bufprint(p, end, " androidboot.console=ttyS%d", shell_serial );
        }

        if (opts->trace) {
            p = bufprint(p, end, " android.tracing=1");
        }

        if (!opts->no_jni) {
            p = bufprint(p, end, " android.checkjni=1");
        }

        if (opts->no_boot_anim) {
            p = bufprint( p, end, " android.bootanim=0" );
        }

        if (opts->logcat) {
            char*  q = bufprint(p, end, " androidboot.logcat=%s", opts->logcat);

            if (q < end) {
                /* replace any space by a comma ! */
                {
                    int  nn;
                    for (nn = 1; p[nn] != 0; nn++)
                        if (p[nn] == ' ' || p[nn] == '\t')
                            p[nn] = ',';
                    p += nn;
                }
            }
            p = q;
        }

        if (opts->bootchart) {
            p = bufprint(p, end, " androidboot.bootchart=%s", opts->bootchart);
        }

        if (p >= end) {
            fprintf(stderr, "### ERROR: kernel parameters too long\n");
            exit(1);
        }

        hw->kernel_parameters = strdup(params);
    }

    if (opts->ports) {
        args[n++] = "-android-ports";
        args[n++] = opts->ports;
    }

    if (opts->port) {
        args[n++] = "-android-port";
        args[n++] = opts->port;
    }

    if (opts->report_console) {
        args[n++] = "-android-report-console";
        args[n++] = opts->report_console;
    }

    if (opts->http_proxy) {
        args[n++] = "-http-proxy";
        args[n++] = opts->http_proxy;
    }

    if (!opts->charmap) {
        /* Try to find a valid charmap name */
        char* charmap = avdInfo_getCharmapFile(avd, hw->hw_keyboard_charmap);
        if (charmap != NULL) {
            D("autoconfig: -charmap %s", charmap);
            opts->charmap = charmap;
        }
    }

    if (opts->charmap) {
        char charmap_name[AKEYCHARMAP_NAME_SIZE];

        if (!path_exists(opts->charmap)) {
            derror("Charmap file does not exist: %s", opts->charmap);
            exit(1);
        }
        /* We need to store the charmap name in the hardware configuration.
         * However, the charmap file itself is only used by the UI component
         * and doesn't need to be set to the emulation engine.
         */
        kcm_extract_charmap_name(opts->charmap, charmap_name,
                                 sizeof(charmap_name));
        AFREE(hw->hw_keyboard_charmap);
        hw->hw_keyboard_charmap = ASTRDUP(charmap_name);
    }

    if (opts->memcheck) {
        args[n++] = "-android-memcheck";
        args[n++] = opts->memcheck;
    }

    if (opts->gpu) {
        const char* gpu = opts->gpu;
        if (!strcmp(gpu,"on") || !strcmp(gpu,"enable")) {
            hw->hw_gpu_enabled = 1;
        } else if (!strcmp(gpu,"off") || !strcmp(gpu,"disable")) {
            hw->hw_gpu_enabled = 0;
        } else if (!strcmp(gpu,"auto")) {
            /* Nothing to do */
        } else {
            derror("Invalid value for -gpu <mode> parameter: %s\n", gpu);
            derror("Valid values are: on, off or auto\n");
            exit(1);
        }
    }

    /* Quit emulator on condition that both, gpu and snapstorage are on. This is
     * a temporary solution preventing the emulator from crashing until GPU state
     * can be properly saved / resored in snapshot file. */
    if (hw->hw_gpu_enabled && opts->snapstorage && (!opts->no_snapshot_load ||
                                                    !opts->no_snapshot_save)) {
        derror("Snapshots and gpu are mutually exclusive at this point. Please turn one of them off, and restart the emulator.");
        exit(1);
    }

    /* Deal with camera emulation */
    if (opts->webcam_list) {
        /* List connected webcameras */
        args[n++] = "-list-webcam";
    }

    if (opts->camera_back) {
        /* Validate parameter. */
        if (memcmp(opts->camera_back, "webcam", 6) &&
            strcmp(opts->camera_back, "emulated") &&
            strcmp(opts->camera_back, "none")) {
            derror("Invalid value for -camera-back <mode> parameter: %s\n"
                   "Valid values are: 'emulated', 'webcam<N>', or 'none'\n",
                   opts->camera_back);
            exit(1);
        }
        hw->hw_camera_back = ASTRDUP(opts->camera_back);
    }

    if (opts->camera_front) {
        /* Validate parameter. */
        if (memcmp(opts->camera_front, "webcam", 6) &&
            strcmp(opts->camera_front, "emulated") &&
            strcmp(opts->camera_front, "none")) {
            derror("Invalid value for -camera-front <mode> parameter: %s\n"
                   "Valid values are: 'emulated', 'webcam<N>', or 'none'\n",
                   opts->camera_front);
            exit(1);
        }
        hw->hw_camera_front = ASTRDUP(opts->camera_front);
    }

    /* physical memory is now in hw->hw_ramSize */

    hw->avd_name = ASTRDUP(avdInfo_getName(avd));

    /* Set up the interfaces for inter-emulator networking */
    if (opts->shared_net_id) {
        unsigned int shared_net_id = atoi(opts->shared_net_id);
        char nic[37];

        args[n++] = "-net";
        args[n++] = "nic,vlan=0";
        args[n++] = "-net";
        args[n++] = "user,vlan=0";

        args[n++] = "-net";
        snprintf(nic, sizeof nic, "nic,vlan=1,macaddr=52:54:00:12:34:%02x", shared_net_id);
        args[n++] = strdup(nic);
        args[n++] = "-net";
        args[n++] = "socket,vlan=1,mcast=230.0.0.10:1234";
    }

    /* Setup screen emulation */
    if (opts->screen) {
        if (strcmp(opts->screen, "touch") &&
            strcmp(opts->screen, "multi-touch") &&
            strcmp(opts->screen, "no-touch")) {

            derror("Invalid value for -screen <mode> parameter: %s\n"
                   "Valid values are: touch, multi-touch, or no-touch\n",
                   opts->screen);
            exit(1);
        }
        hw->hw_screen = ASTRDUP(opts->screen);
    }

    while(argc-- > 0) {
        args[n++] = *argv++;
    }
    args[n] = 0;

    /* If the target ABI is armeabi-v7a, we can auto-detect the cpu model
     * as a cortex-a8, instead of the default (arm926) which only emulates
     * an ARMv5TE CPU.
     */
    if (!forceArmv7 && hw->hw_cpu_model[0] == '\0')
    {
        char* abi = avdInfo_getTargetAbi(avd);
        if (abi != NULL) {
            if (!strcmp(abi, "armeabi-v7a")) {
                forceArmv7 = 1;
            }
            AFREE(abi);
        }
    }

    if (forceArmv7 != 0) {
        AFREE(hw->hw_cpu_model);
        hw->hw_cpu_model = ASTRDUP("cortex-a8");
        D("Auto-config: -qemu -cpu %s", hw->hw_cpu_model);
    }

    /* Generate a hardware-qemu.ini for this AVD. The real hardware
     * configuration is ususally stored in several files, e.g. the AVD's
     * config.ini plus the skin-specific hardware.ini.
     *
     * The new file will group all definitions and will be used to
     * launch the core with the -android-hw <file> option.
     */
    {
        const char* coreHwIniPath = avdInfo_getCoreHwIniPath(avd);
        IniFile*    hwIni         = iniFile_newFromMemory("", NULL);
        androidHwConfig_write(hw, hwIni);

        if (filelock_create(coreHwIniPath) == NULL) {
            /* The AVD is already in use, we still support this as an
             * experimental feature. Use a temporary hardware-qemu.ini
             * file though to avoid overwriting the existing one. */
             TempFile*  tempIni = tempfile_create();
             coreHwIniPath = tempfile_path(tempIni);
        }

        /* While saving HW config, ignore valueless entries. This will not break
         * anything, but will significantly simplify comparing the current HW
         * config with the one that has been associated with a snapshot (in case
         * VM starts from a snapshot for this instance of emulator). */
        if (iniFile_saveToFileClean(hwIni, coreHwIniPath) < 0) {
            derror("Could not write hardware.ini to %s: %s", coreHwIniPath, strerror(errno));
            exit(2);
        }
        args[n++] = "-android-hw";
        args[n++] = strdup(coreHwIniPath);

        /* In verbose mode, dump the file's content */
        if (VERBOSE_CHECK(init)) {
            FILE* file = fopen(coreHwIniPath, "rt");
            if (file == NULL) {
                derror("Could not open hardware configuration file: %s\n",
                       coreHwIniPath);
            } else {
                LineInput* input = lineInput_newFromStdFile(file);
                const char* line;
                printf("Content of hardware configuration file:\n");
                while ((line = lineInput_getLine(input)) !=  NULL) {
                    printf("  %s\n", line);
                }
                printf(".\n");
                lineInput_free(input);
                fclose(file);
            }
        }
    }

    if(VERBOSE_CHECK(init)) {
        int i;
        printf("QEMU options list:\n");
        for(i = 0; i < n; i++) {
            printf("emulator: argv[%02d] = \"%s\"\n", i, args[i]);
        }
        /* Dump final command-line option to make debugging the core easier */
        printf("Concatenated QEMU options:\n");
        for (i = 0; i < n; i++) {
            /* To make it easier to copy-paste the output to a command-line,
             * quote anything that contains spaces.
             */
            if (strchr(args[i], ' ') != NULL) {
                printf(" '%s'", args[i]);
            } else {
                printf(" %s", args[i]);
            }
        }
        printf("\n");
    }

    /* Setup SDL UI just before calling the code */
    init_sdl_ui(skinConfig, skinPath, opts);

    if (attach_ui_to_core(opts) < 0) {
        derror("Can't attach to core!");
        exit(1);
    }

    return qemu_main(n, args);
}
