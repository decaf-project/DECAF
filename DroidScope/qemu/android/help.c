#include "android/help.h"
#include "android/cmdline-option.h"
#include "android/utils/path.h"
#include "android/utils/bufprint.h"
#include "android/utils/debug.h"
#include "android/utils/misc.h"
#include "android/skin/keyset.h"
#include "android/android.h"
#include <stdint.h>
#include "audio/audio.h"
#include <string.h>
#include <stdlib.h>
#include "android/protocol/core-commands-api.h"

/* XXX: TODO: put most of the help stuff in auto-generated files */

#define  PRINTF(...)  stralloc_add_format(out,__VA_ARGS__)

static void
help_virtual_device( stralloc_t*  out )
{
    PRINTF(
    "  An Android Virtual Device (AVD) models a single virtual\n"
    "  device running the Android platform that has, at least, its own\n"
    "  kernel, system image and data partition.\n\n"

    "  Only one emulator process can run a given AVD at a time, but\n"
    "  you can create several AVDs and run them concurrently.\n\n"

    "  You can invoke a given AVD at startup using either '-avd <name>'\n"
    "  or '@<name>', both forms being equivalent. For example, to launch\n"
    "  the AVD named 'foo', type:\n\n"

    "      emulator @foo\n\n"

    "  The 'android' helper tool can be used to manage virtual devices.\n"
    "  For example:\n\n"

    "    android create avd -n <name> -t 1  # creates a new virtual device.\n"
    "    android list avd                   # list all virtual devices available.\n\n"

    "  Try 'android --help' for more commands.\n\n"

    "  Each AVD really corresponds to a content directory which stores\n"
    "  persistent and writable disk images as well as configuration files.\n"

    "  Each AVD must be created against an existing SDK platform or add-on.\n"
    "  For more information on this topic, see -help-sdk-images.\n\n"

    "  SPECIAL NOTE: in the case where you are *not* using the emulator\n"
    "  with the Android SDK, but with the Android build system, you will\n"
    "  need to define the ANDROID_PRODUCT_OUT variable in your environment.\n"
    "  See -help-build-images for the details.\n"
    );
}


static void
help_sdk_images( stralloc_t*  out )
{
    PRINTF(
    "  The Android SDK now supports multiple versions of the Android platform.\n"
    "  Each SDK 'platform' corresponds to:\n\n"

    "    - a given version of the Android API.\n"
    "    - a set of corresponding system image files.\n"
    "    - build and configuration properties.\n"
    "    - an android.jar file used when building your application.\n"
    "    - skins.\n\n"

    "  The Android SDK also supports the concept of 'add-ons'. Each add-on is\n"
    "  based on an existing platform, and provides replacement or additional\n"
    "  image files, android.jar, hardware configuration options and/or skins.\n\n"

    "  The purpose of add-ons is to allow vendors to provide their own customized\n"
    "  system images and APIs without needing to package a complete SDK.\n\n"

    "  Before using the SDK, you need to create an Android Virtual Device (AVD)\n"
    "  (see -help-virtual-device for details). Each AVD is created in reference\n"
    "  to a given SDK platform *or* add-on, and will search the corresponding\n"
    "  directories for system image files, in the following order:\n\n"

    "    - in the AVD's content directory.\n"
    "    - in the AVD's SDK add-on directory, if any.\n"
    "    - in the AVD's SDK platform directory, if any.\n\n"

    "  The image files are documented in -help-disk-images. By default, an AVD\n"
    "  content directory will contain the following persistent image files:\n\n"

    "     userdata-qemu.img     - the /data partition image file\n"
    "     cache.img             - the /cache partition image file\n\n"

    "  You can use -wipe-data to re-initialize the /data partition to its factory\n"
    "  defaults. This will erase all user settings for the virtual device.\n\n"
    );
}

static void
help_build_images( stralloc_t*  out )
{
    PRINTF(
    "  The emulator detects that you are working from the Android build system\n"
    "  by looking at the ANDROID_PRODUCT_OUT variable in your environment.\n\n"

    "  If it is defined, it should point to the product-specific directory that\n"
    "  contains the generated system images.\n"

    "  In this case, the emulator will look by default for the following image\n"
    "  files there:\n\n"

    "    - system.img   : the *initial* system image.\n"
    "    - ramdisk.img  : the ramdisk image used to boot the system.\n"
    "    - userdata.img : the *initial* user data image (see below).\n"
    "    - kernel-qemu  : the emulator-specific Linux kernel image.\n\n"

    "  If the kernel image is not found in the out directory, then it is searched\n"
    "  in <build-root>/prebuilts/qemu-kernel/.\n\n"

    "  Skins will be looked in <build-root>/development/tools/emulator/skins/\n\n"

    "  You can use the -sysdir, -system, -kernel, -ramdisk, -datadir, -data options\n"
    "  to specify different search directories or specific image files. You can\n"
    "  also use the -cache and -sdcard options to indicate specific cache partition\n"
    "  and SD Card image files.\n\n"

    "  For more details, see the corresponding -help-<option> section.\n\n"

    "  Note that the following behaviour is specific to 'build mode':\n\n"

    "  - the *initial* system image is copied to a temporary file which is\n"
    "    automatically removed when the emulator exits. There is thus no way to\n"
    "    make persistent changes to this image through the emulator, even if\n"
    "    you use the '-image <file>' option.\n\n"

    "  - unless you use the '-cache <file>' option, the cache partition image\n"
    "    is backed by a temporary file that is initially empty and destroyed on\n"
    "    program exit.\n\n"

    "  SPECIAL NOTE: If you are using the emulator with the Android SDK, the\n"
    "  information above doesn't apply. See -help-sdk-images for more details.\n"
    );
}

static void
help_disk_images( stralloc_t*  out )
{
    char  datadir[256];

    bufprint_config_path( datadir, datadir + sizeof(datadir) );

    PRINTF(
    "  The emulator needs several key image files to run appropriately.\n"
    "  Their exact location depends on whether you're using the emulator\n"
    "  from the Android SDK, or not (more details below).\n\n"

    "  The minimal required image files are the following:\n\n"

    "    kernel-qemu      the emulator-specific Linux kernel image\n"
    "    ramdisk.img      the ramdisk image used to boot the system\n"
    "    system.img       the *initial* system image\n"
    "    userdata.img     the *initial* data partition image\n\n"

    "  It will also use the following writable image files:\n\n"

    "    userdata-qemu.img  the persistent data partition image\n"
    "    system-qemu.img    an *optional* persistent system image\n"
    "    cache.img          an *optional* cache partition image\n"
    "    sdcard.img         an *optional* SD Card partition image\n\n"
    "    snapshots.img      an *optional* state snapshots image\n\n"

    "  If you use a virtual device, its content directory should store\n"
    "  all writable images, and read-only ones will be found from the\n"
    "  corresponding platform/add-on directories. See -help-sdk-images\n"
    "  for more details.\n\n"

    "  If you are building from the Android build system, you should\n"
    "  have ANDROID_PRODUCT_OUT defined in your environment, and the\n"
    "  emulator shall be able to pick-up the right image files automatically.\n"
    "  See -help-build-images for more details.\n\n"

    "  If you're neither using the SDK or the Android build system, you\n"
    "  can still run the emulator by explicitely providing the paths to\n"
    "  *all* required disk images through a combination of the following\n"
    "  options: -sysdir, -datadir, -kernel, -ramdisk, -system, -data, -cache\n"
    "  -sdcard and -snapstorage.\n\n"

    "  The actual logic being that the emulator should be able to find all\n"
    "  images from the options you give it.\n\n"

    "  For more detail, see the corresponding -help-<option> entry.\n\n"

    "  Other related options are:\n\n"

    "      -init-data       Specify an alternative *initial* user data image\n\n"

    "      -wipe-data       Copy the content of the *initial* user data image\n"
"                           (userdata.img) into the writable one (userdata-qemu.img)\n\n"

    "      -no-cache        do not use a cache partition, even if one is\n"
    "                       available.\n\n"

    "      -no-snapstorage  do not use a state snapshot image, even if one is\n"
    "                       available.\n\n"
    ,
    datadir );
}

static void
help_keys(stralloc_t*  out)
{
    int  pass, maxw = 0;

    stralloc_add_str( out, "  When running the emulator, use the following keypresses:\n\n");

    if (!android_keyset)
        android_keyset = skin_keyset_new_from_text( skin_keyset_get_default() );

    for (pass = 0; pass < 2; pass++) {
        SkinKeyCommand  cmd;

        for (cmd = SKIN_KEY_COMMAND_NONE+1; cmd < SKIN_KEY_COMMAND_MAX; cmd++)
        {
            SkinKeyBinding  bindings[ SKIN_KEY_COMMAND_MAX_BINDINGS ];
            int             n, count, len;
            char            temp[32], *p = temp, *end = p + sizeof(temp);

            count = skin_keyset_get_bindings( android_keyset, cmd, bindings );
            if (count <= 0)
                continue;

            for (n = 0; n < count; n++) {
                p = bufprint(p, end, "%s%s", (n == 0) ? "" : ", ",
                            skin_key_symmod_to_str( bindings[n].sym, bindings[n].mod ) );
            }

            if (pass == 0) {
                len = strlen(temp);
                if (len > maxw)
                    maxw = len;
            } else {
                PRINTF( "    %-*s  %s\n", maxw, temp, skin_key_command_description(cmd) );
            }
        }
    }
    PRINTF( "\n" );
    PRINTF( "  note that NumLock must be deactivated for keypad keys to work\n\n" );
}


static void
help_environment(stralloc_t*  out)
{
    PRINTF(
    "  The Android emulator looks at various environment variables when it starts:\n\n"

    "  If ANDROID_LOG_TAGS is defined, it will be used as in '-logcat <tags>'.\n\n"

    "  If 'http_proxy' is defined, it will be used as in '-http-proxy <proxy>'.\n\n"

    "  If ANDROID_VERBOSE is defined, it can contain a comma-separated list of\n"
    "  verbose items. for example:\n\n"

    "      ANDROID_VERBOSE=socket,radio\n\n"

    "  is equivalent to using the '-verbose -verbose-socket -verbose-radio'\n"
    "  options together. unsupported items will be ignored.\n\n"

    "  If ANDROID_LOG_TAGS is defined, it will be used as in '-logcat <tags>'.\n\n"

    "  If ANDROID_SDK_HOME is defined, it indicates the path of the '.android'\n"
    "  directory which contains the SDK user data (Android Virtual Devices,\n"
    "  DDMS preferences, key stores, etc.).\n\n"

    "  If ANDROID_SDK_ROOT is defined, it indicates the path of the SDK\n"
    "  installation directory.\n\n"

    );
}


static void
help_keyset_file(stralloc_t*  out)
{
    int           n, count;
    const char**  strings;
    char          temp[MAX_PATH];

    PRINTF(
    "  on startup, the emulator looks for 'keyset' file that contains the\n"
    "  configuration of key-bindings to use. the default location on this\n"
    "  system is:\n\n"
    );

    bufprint_config_file( temp, temp+sizeof(temp), KEYSET_FILE );
    PRINTF( "    %s\n\n", temp );

    PRINTF(
    "  if the file doesn't exist, the emulator writes one containing factory\n"
    "  defaults. you are then free to modify it to suit specific needs.\n\n"
    "  this file shall contain a list of text lines in the following format:\n\n"

    "    <command> [<modifiers>]<key>\n\n"

    "  where <command> is an emulator-specific command name, i.e. one of:\n\n"
    );

    count   = SKIN_KEY_COMMAND_MAX-1;
    strings = calloc( count, sizeof(char*) );
    for (n = 0; n < count; n++)
        strings[n] = skin_key_command_to_str(n+1);

    stralloc_tabular( out, strings, count, "    ", 80-8 );
    free(strings);

    PRINTF(
    "\n"
    "  <modifers> is an optional list of <modifier> elements (without separators)\n"
    "  which can be one of:\n\n"

    "    Ctrl-     Left Control Key\n"
    "    Shift-    Left Shift Key\n"
    "    Alt-      Left Alt key\n"
    "    RCtrl-    Right Control Key\n"
    "    RShift-   Right Shift Key\n"
    "    RAlt-     Right Alt key (a.k.a AltGr)\n"
    "\n"
    "  finally <key> is a QWERTY-specific keyboard symbol which can be one of:\n\n"
    );
    count   = skin_keysym_str_count();
    strings = calloc( count, sizeof(char*) );
    for (n = 0; n < count; n++)
        strings[n] = skin_keysym_str(n);

    stralloc_tabular( out, strings, count, "    ", 80-8 );
    free(strings);

    PRINTF(
    "\n"
    "  case is not significant, and a single command can be associated to up\n"
    "  to %d different keys. to bind a command to multiple keys, use commas to\n"
    "  separate them. here are some examples:\n\n",
    SKIN_KEY_COMMAND_MAX_BINDINGS );

    PRINTF(
    "    TOGGLE_NETWORK      F8                # toggle the network on/off\n"
    "    CHANGE_LAYOUT_PREV  Keypad_7,Ctrl-J   # switch to a previous skin layout\n"
    "\n"
    );
}


static void
help_debug_tags(stralloc_t*  out)
{
    int  n;

#define  _VERBOSE_TAG(x,y)   { #x, VERBOSE_##x, y },
    static const struct { const char*  name; int  flag; const char*  text; }
    verbose_options[] = {
        VERBOSE_TAG_LIST
        { 0, 0, 0 }
    };
#undef _VERBOSE_TAG

    PRINTF(
    "  the '-debug <tags>' option can be used to enable or disable debug\n"
    "  messages from specific parts of the emulator. <tags> must be a list\n"
    "  (separated by space/comma/column) of <component> names, which can be one of:\n\n"
    );

    for (n = 0; n < VERBOSE_MAX; n++)
        PRINTF( "    %-12s    %s\n", verbose_options[n].name, verbose_options[n].text );
    PRINTF( "    %-12s    %s\n", "all", "all components together\n" );

    PRINTF(
    "\n"
    "  each <component> can be prefixed with a single '-' to indicate the disabling\n"
    "  of its debug messages. for example:\n\n"

    "    -debug all,-socket,-keys\n\n"

    "  enables all debug messages, except the ones related to network sockets\n"
    "  and key bindings/presses\n\n"
    );
}

static void
help_char_devices(stralloc_t*  out)
{
    PRINTF(
    "  various emulation options take a <device> specification that can be used to\n"
    "  specify something to hook to an emulated device or communication channel.\n"
    "  here is the list of supported <device> specifications:\n\n"

    "      stdio\n"
    "          standard input/output. this may be subject to character\n"
    "          translation (e.g. LN <=> CR/LF)\n\n"

    "      COM<n>   [Windows only]\n"
    "          where <n> is a digit. host serial communication port.\n\n"

    "      pipe:<filename>\n"
    "          named pipe <filename>\n\n"

    "      file:<filename>\n"
    "          write output to <filename>, no input can be read\n\n"

    "      pty  [Linux only]\n"
    "          pseudo TTY (a new PTY is automatically allocated)\n\n"

    "      /dev/<file>  [Unix only]\n"
    "          host char device file, e.g. /dev/ttyS0. may require root access\n\n"

    "      /dev/parport<N>  [Linux only]\n"
    "          use host parallel port. may require root access\n\n"

    "      unix:<path>[,server][,nowait]]     [Unix only]\n"
    "          use a Unix domain socket. if you use the 'server' option, then\n"
    "          the emulator will create the socket and wait for a client to\n"
    "          connect before continuing, unless you also use 'nowait'\n\n"

    "      tcp:[<host>]:<port>[,server][,nowait][,nodelay]\n"
    "          use a TCP socket. 'host' is set to localhost by default. if you\n"
    "          use the 'server' option will bind the port and wait for a client\n"
    "          to connect before continuing, unless you also use 'nowait'. the\n"
    "          'nodelay' option disables the TCP Nagle algorithm\n\n"

    "      telnet:[<host>]:<port>[,server][,nowait][,nodelay]\n"
    "          similar to 'tcp:' but uses the telnet protocol instead of raw TCP\n\n"

    "      udp:[<remote_host>]:<remote_port>[@[<src_ip>]:<src_port>]\n"
    "          send output to a remote UDP server. if 'remote_host' is no\n"
    "          specified it will default to '0.0.0.0'. you can also receive input\n"
    "          through UDP by specifying a source address after the optional '@'.\n\n"

    "      fdpair:<fd1>,<fd2>  [Unix only]\n"
    "          redirection input and output to a pair of pre-opened file\n"
    "          descriptors. this is mostly useful for scripts and other\n"
    "          programmatic launches of the emulator.\n\n"

    "      none\n"
    "          no device connected\n\n"

    "      null\n"
    "          the null device (a.k.a /dev/null on Unix, or NUL on Win32)\n\n"

    "  NOTE: these correspond to the <device> parameter of the QEMU -serial option\n"
    "        as described on http://bellard.org/qemu/qemu-doc.html#SEC10\n\n"
    );
}

static void
help_avd(stralloc_t*  out)
{
    PRINTF(
    "  use '-avd <name>' to start the emulator program with a given Android\n"
    "  Virtual Device (a.k.a. AVD), where <name> must correspond to the name\n"
    "  of one of the existing AVDs available on your host machine.\n\n"

      "See -help-virtual-device to learn how to create/list/manage AVDs.\n\n"

    "  As a special convenience, using '@<name>' is equivalent to using\n"
    "  '-avd <name>'.\n\n"
    );
}

static void
help_sysdir(stralloc_t*  out)
{
    char   systemdir[MAX_PATH];
    char   *p = systemdir, *end = p + sizeof(systemdir);

    p = bufprint_app_dir( p, end );
    p = bufprint( p, end, PATH_SEP "lib" PATH_SEP "images" );

    PRINTF(
    "  use '-sysdir <dir>' to specify a directory where system read-only\n"
    "  image files will be searched. on this system, the default directory is:\n\n"
    "      %s\n\n", systemdir );

    PRINTF(
    "  see '-help-disk-images' for more information about disk image files\n\n" );
}

static void
help_datadir(stralloc_t*  out)
{
    char  datadir[MAX_PATH];

    bufprint_config_path(datadir, datadir + sizeof(datadir));

    PRINTF(
    "  use '-datadir <dir>' to specify a directory where writable image files\n"
    "  will be searched. on this system, the default directory is:\n\n"
    "      %s\n\n", datadir );

    PRINTF(
    "  see '-help-disk-images' for more information about disk image files\n\n" );
}

static void
help_kernel(stralloc_t*  out)
{
    PRINTF(
    "  use '-kernel <file>' to specify a Linux kernel image to be run.\n"
    "  the default image is 'kernel-qemu' from the system directory.\n\n"

    "  you can use '-debug-kernel' to see debug messages from the kernel\n"
    "  to the terminal\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_ramdisk(stralloc_t*  out)
{
    PRINTF(
    "  use '-ramdisk <file>' to specify a Linux ramdisk boot image to be run in\n"
    "  the emulator. the default image is 'ramdisk.img' from the system directory.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_system(stralloc_t*  out)
{
    PRINTF(
    "  use '-system <file>' to specify the intial system image that will be loaded.\n"
    "  the default image is 'system.img' from the system directory.\n\n"

    "  NOTE: In previous releases of the Android SDK, this option was named '-image'.\n"
    "        And using '-system <path>' was equivalent to using '-sysdir <path>' now.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_image(stralloc_t*  out)
{
    PRINTF(
    "  This option is obsolete, you should use '-system <file>' instead to point\n"
    "  to the initial system image.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_data(stralloc_t*  out)
{
    PRINTF(
    "  use '-data <file>' to specify a different /data partition image file.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_wipe_data(stralloc_t*  out)
{
    PRINTF(
    "  use '-wipe-data' to reset your /data partition image to its factory\n"
    "  defaults. this removes all installed applications and settings.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_cache(stralloc_t*  out)
{
    PRINTF(
    "  use '-cache <file>' to specify a /cache partition image. if <file> does\n"
    "  not exist, it will be created empty. by default, the cache partition is\n"
    "  backed by a temporary file that is deleted when the emulator exits.\n"
    "  using the -cache option allows it to be persistent.\n\n"

    "  the '-no-cache' option can be used to disable the cache partition.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_cache_size(stralloc_t*  out)
{
    PRINTF(
    "  use '-cache <size>' to specify a /cache partition size in MB. By default,\n"
    "  the cache partition size is set to 66MB\n\n"
    );
}

static void
help_no_cache(stralloc_t*  out)
{
    PRINTF(
    "  use '-no-cache' to disable the cache partition in the emulated system.\n"
    "  the cache partition is optional, but when available, is used by the browser\n"
    "  to cache web pages and images\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_sdcard(stralloc_t*  out)
{
    PRINTF(
    "  use '-sdcard <file>' to specify a SD Card image file that will be attached\n"
    "  to the emulator. By default, the 'sdcard.img' file is searched in the data\n"
    "  directory.\n\n"

    "  if the file does not exist, the emulator will still start, but without an\n"
    "  attached SD Card.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n\n"
    );
}

static void
help_snapstorage(stralloc_t*  out)
{
    PRINTF(
    "  Use '-snapstorage <file>' to specify a repository file for snapshots.\n"
    "  All snapshots made during execution will be saved in this file, and only\n"
    "  snapshots in this file can be restored during the emulator run.\n\n"

    "  If the option is not specified, it defaults to 'snapshots.img' in the\n"
    "  data directory. If the specified file does not exist, the emulator will\n"
    "  start, but without support for saving or loading state snapshots.\n\n"

    "  see '-help-disk-images' for more information about disk image files\n"
    "  see '-help-snapshot' for more information about snapshots\n\n"
    );
}

static void
help_no_snapstorage(stralloc_t*  out)
{
    PRINTF(
    "  This starts the emulator without mounting a file to store or load state\n"
    "  snapshots, forcing a full boot and disabling state snapshot functionality.\n\n"
    ""
    "  This command overrides the configuration specified by the parameters\n"
    "  '-snapstorage' and '-snapshot'. A warning will be raised if either\n"
    "  of those parameters was specified anyway.\n\n"
     );
 }

static void
help_snapshot(stralloc_t*  out)
{
    PRINTF(
    "  Rather than executing a full boot sequence, the Android emulator can\n"
    "  resume execution from an earlier state snapshot (which is usually\n"
    "  significantly faster). When the parameter '-snapshot <name>' is given,\n"
    "  the emulator loads the snapshot of that name from the snapshot image,\n"
    "  and saves it back under the same name on exit.\n\n"

    "  If the option is not specified, it defaults to 'default-boot'. If the\n"
    "  specified snapshot does not exist, the emulator will perform a full boot\n"
    "  sequence instead, but will still save.\n\n"

    "  WARNING: In the process of loading, all contents of the system, userdata\n"
    "           and SD card images will be OVERWRITTEN with the contents they\n"
    "           held when the snapshot was made. Unless saved in a different\n"
    "           snapshot, any changes since will be lost!\n\n"

    "  If you want to create a snapshot manually, connect to the emulator console:\n\n"

    "      telnet localhost <port>\n\n"

    "  Then execute the command 'avd snapshot save <name>'. See '-help-port' for\n"
    "  information on obtaining <port>.\n\n"
    );
}

static void
help_no_snapshot(stralloc_t*  out)
{
    PRINTF(
    "  This inhibits both the autoload and autosave operations, forcing\n"
    "  emulator to perform a full boot sequence and losing state on close.\n"
    "  It overrides the '-snapshot' parameter.\n"
    "  If '-snapshot' was specified anyway, a warning is raised.\n\n"
    );
}

static void
help_no_snapshot_load(stralloc_t*  out)
{
    PRINTF(
    "  Prevents the emulator from loading the AVD's state from the snapshot\n"
    "  storage on start.\n\n"
    );
}

static void
help_no_snapshot_save(stralloc_t*  out)
{
    PRINTF(
    "  Prevents the emulator from saving the AVD's state to the snapshot\n"
    "  storage on exit, meaning that all changes will be lost.\n\n"
    );
}

static void
help_no_snapshot_update_time(stralloc_t*  out)
{
    PRINTF(
    "  Prevent the emulator from sending an unsolicited time update\n"
    "  in response to the first signal strength query after loadvm,\n"
    "  to avoid a sudden time jump that might upset testing. (Signal\n"
    "  strength is queried approximately every 15 seconds)\n\n"
    );
}

static void
help_snapshot_list(stralloc_t*  out)
{
    PRINTF(
    "  This prints a table of snapshots that are stored in the snapshot storage\n"
    "  file that the emulator was started with, then exits. Values from the 'ID'\n"
    "  and 'TAG' columns can be used as arguments for the '-snapshot' parameter.\n\n"

    "  If '-snapstorage <file>' was specified as well, this command prints a "
    "  table of the snapshots stored in <file>.\n\n"

    "  See '-help-snapshot' for more information on snapshots.\n\n"
    );
}

static void
help_skindir(stralloc_t*  out)
{
    PRINTF(
    "  use '-skindir <dir>' to specify a directory that will be used to search\n"
    "  for emulator skins. each skin must be a subdirectory of <dir>. by default\n"
    "  the emulator will look in the 'skins' sub-directory of the system directory\n\n"

    "  the '-skin <name>' option is required when -skindir is used.\n"
    );
}

static void
help_skin(stralloc_t*  out)
{
    PRINTF(
    "  use '-skin <skin>' to specify an emulator skin, each skin corresponds to\n"
    "  the visual appearance of a given device, including buttons and keyboards,\n"
    "  and is stored as subdirectory <skin> of the skin root directory\n"
    "  (see '-help-skindir')\n\n" );

    PRINTF(
    "  note that <skin> can also be '<width>x<height>' (e.g. '320x480') to\n"
    "  specify an exact framebuffer size, without any visual ornaments.\n\n" );
}

static void
help_dynamic_skin(stralloc_t* out)
{
    PRINTF(
    "  use '-dynamic_skin' to dynamically generate a skin based on the settings\n"
    "  in the AVD. This option only has effect if the -skin WxH option is used\n"
    "  to specify the width and height of the framebuffer\n");
}

/* default network settings for emulator */
#define  DEFAULT_NETSPEED  "full"
#define  DEFAULT_NETDELAY  "none"

static void
help_shaper(stralloc_t*  out)
{
    int  n;
    NetworkSpeed* android_netspeed;
    NetworkLatency* android_netdelay;
    PRINTF(
    "  the Android emulator supports network throttling, i.e. slower network\n"
    "  bandwidth as well as higher connection latencies. this is done either through\n"
    "  skin configuration, or with '-netspeed <speed>' and '-netdelay <delay>'.\n\n"

    "  the format of -netspeed is one of the following (numbers are kbits/s):\n\n" );

    for (n = 0; !corecmd_get_netspeed(n, &android_netspeed); n++) {
        PRINTF( "    -netspeed %-12s %-15s  (up: %.1f, down: %.1f)\n",
                        android_netspeed->name,
                        android_netspeed->display,
                        android_netspeed->upload/1000.,
                        android_netspeed->download/1000. );
        free(android_netspeed);
    }
    PRINTF( "\n" );
    PRINTF( "    -netspeed %-12s %s", "<num>", "select both upload and download speed\n");
    PRINTF( "    -netspeed %-12s %s", "<up>:<down>", "select individual up and down speed\n");

    PRINTF( "\n  The format of -netdelay is one of the following (numbers are msec):\n\n" );
    for (n = 0; !corecmd_get_netdelay(n, &android_netdelay); n++) {
        PRINTF( "    -netdelay %-10s   %-15s  (min %d, max %d)\n",
                        android_netdelay->name, android_netdelay->display,
                        android_netdelay->min_ms, android_netdelay->max_ms );
        free(android_netdelay);
    }
    PRINTF( "    -netdelay %-10s   %s", "<num>", "select exact latency\n");
    PRINTF( "    -netdelay %-10s   %s", "<min>:<max>", "select min and max latencies\n\n");

    PRINTF( "  the emulator uses the following defaults:\n\n" );
    PRINTF( "    Default network speed   is '%s'\n",   DEFAULT_NETSPEED);
    PRINTF( "    Default network latency is '%s'\n\n", DEFAULT_NETDELAY);
}

static void
help_http_proxy(stralloc_t*  out)
{
    PRINTF(
    "  the Android emulator allows you to redirect all TCP connections through\n"
    "  a HTTP/HTTPS proxy. this can be enabled by using the '-http-proxy <proxy>'\n"
    "  option, or by defining the 'http_proxy' environment variable.\n\n"

    "  <proxy> can be one of the following:\n\n"
    "    http://<server>:<port>\n"
    "    http://<username>:<password>@<server>:<port>\n\n"

    "  the 'http://' prefix can be omitted. If '-http-proxy <proxy>' is not used,\n"
    "  the 'http_proxy' environment variable is looked up and any value matching\n"
    "  the <proxy> format will be used automatically\n\n" );
}

static void
help_report_console(stralloc_t*  out)
{
    PRINTF(
    "  the '-report-console <socket>' option can be used to report the\n"
    "  automatically-assigned console port number to a remote third-party\n"
    "  before starting the emulation. <socket> must be in one of these\n"
    "  formats:\n\n"

    "      tcp:<port>[,server][,max=<seconds>]\n"
    "      unix:<path>[,server][,max=<seconds>]\n"
    "\n"
    "  if the 'server' option is used, the emulator opens a server socket\n"
    "  and waits for an incoming connection to it. by default, it will instead\n"
    "  try to make a normal client connection to the socket, and, in case of\n"
    "  failure, will repeat this operation every second for 10 seconds.\n"
    "  the 'max=<seconds>' option can be used to modify the timeout\n\n"

    "  when the connection is established, the emulator sends its console port\n"
    "  number as text to the remote third-party, then closes the connection and\n"
    "  starts the emulation as usual. *any* failure in the process described here\n"
    "  will result in the emulator aborting immediately\n\n"

    "  as an example, here's a small Unix shell script that starts the emulator in\n"
    "  the background and waits for its port number with the help of the 'netcat'\n"
    "  utility:\n\n"

    "      MYPORT=5000\n"
    "      emulator -no-window -report-console tcp:$MYPORT &\n"
    "      CONSOLEPORT=`nc -l localhost $MYPORT`\n"
    "\n"
    );
}

static void
help_dpi_device(stralloc_t*  out)
{
    PRINTF(
    "  use '-dpi-device <dpi>' to specify the screen resolution of the emulated\n"
    "  device. <dpi> must be an integer between 72 and 1000. the default is taken\n"
    "  from the skin, if available, or uses the contant value %d (an average of\n"
    "  several prototypes used during Android development).\n\n", DEFAULT_DEVICE_DPI );

    PRINTF(
    "  the device resolution can also used to rescale the emulator window with\n"
    "  the '-scale' option (see -help-scale)\n\n"
    );
}

static void
help_audio(stralloc_t*  out)
{
    PRINTF(
    "  the '-audio <backend>' option allows you to select a specific backend\n"
    "  to be used to both play and record audio in the Android emulator.\n\n"

    "  use '-audio none' to disable audio completely.\n\n"
    );
}

static void
help_scale(stralloc_t*  out)
{
    PRINTF(
    "  the '-scale <scale>' option is used to scale the emulator window to\n"
    "  something that better fits the physical dimensions of a real device. this\n"
    "  can be *very* useful to check that your UI isn't too small to be usable\n"
    "  on a real device.\n\n"

    "  there are three supported formats for <scale>:\n\n"

    "  * if <scale> is a real number (between 0.1 and 3.0) it is used as a\n"
    "    scaling factor for the emulator's window.\n\n"

    "  * if <scale> is an integer followed by the suffix 'dpi' (e.g. '110dpi'),\n"
    "    then it is interpreted as the resolution of your monitor screen. this\n"
    "    will be divided by the emulated device's resolution to get an absolute\n"
    "    scale. (see -help-dpi-device for details).\n\n"

    "  * finally, if <scale> is the keyword 'auto', the emulator tries to guess\n"
    "    your monitor's resolution and automatically adjusts its window\n"
    "    accordingly\n\n"

    "    NOTE: this process is *very* unreliable, depending on your OS, video\n"
    "          driver issues and other random system parameters\n\n"

    "  the emulator's scale can be changed anytime at runtime through the control\n"
    "  console. see the help for the 'window scale' command for details\n\n" );
}

static void
help_trace(stralloc_t*  out)
{
    PRINTF(
    "  use '-trace <name>' to start the emulator with runtime code profiling support\n"
    "  profiling itself will not be enabled unless you press F9 to activate it, or\n"
    "  the executed code turns it on programmatically.\n\n"

    "  trace information is stored in directory <name>, several files are created\n"
    "  there, that can later be used with the 'traceview' program that comes with\n"
    "  the Android SDK for analysis.\n\n"

    "  note that execution will be slightly slower when enabling code profiling,\n"
    "  this is a necessary requirement of the operations being performed to record\n"
    "  the execution trace. this slowdown should not affect your system until you\n"
    "  enable the profiling though...\n\n"
    );
}

#ifdef CONFIG_MEMCHECK
static void
help_memcheck(stralloc_t*  out)
{
    PRINTF(
    "  use '-memcheck <flags>' to start the emulator with memory access checking\n"
    "  support.\n\n"

    "  <flags> enables, or disables memory access checking, and also controls\n"
    "  what events are going to be logged by the memory access checker.\n"
    "  <flags> can be one of the following:\n"
    "  1 - Enables memory access checking with default logging (\"LIRW\"), or\n"
    "  0 - Disables memory access checking, or\n"
    "  A combination (in no particular order) of the following:\n"
    "     L - Logs memory leaks on process exit.\n"
    "     I - Logs attempts to use invalid pointers in free, or realloc routines.\n"
    "     R - Logs memory access violation on read operations.\n"
    "     W - Logs memory access violation on write operations.\n"
    "     N - Logs new process ID allocation.\n"
    "     F - Logs guest's process forking.\n"
    "     S - Logs guest's process starting.\n"
    "     E - Logs guest's process exiting.\n"
    "     C - Logs guest's thread creation (clone).\n"
    "     B - Logs libc.so initialization in the guest system.\n"
    "     M - Logs module mapping and unmapping in the guest system.\n"
    "     A - Logs all emulator events. Equala to \"LIRWFSECANBM\" combination.\n"
    "     e - Logs error messages, received from the guest system.\n"
    "     d - Logs debug messages, received from the guest system.\n"
    "     i - Logs information messages, received from the guest system.\n"
    "     a - Logs all messages, received from the guest system.\n"
    "         This is equal to \"edi\" combination.\n\n"

    "  note that execution might be significantly slower when enabling memory access\n"
    "  checking, this is a necessary requirement of the operations being performed\n"
    "  to analyze memory allocations and memory access.\n\n"
    );
}
#endif  // CONFIG_MEMCHECK

#ifdef CONFIG_STANDALONE_UI
static void
help_list_cores(stralloc_t*  out)
{
    PRINTF(
    "  use '-list-cores localhost to list emulator core processes running on this machine.\n"
    "  use '-list-cores host_name, or IP address to list emulator core processes running on\n"
    "  a remote machine.\n"
    );
}

static void
help_attach_core(stralloc_t*  out)
{
    PRINTF(
    "  the -attach-core <console socket> options attaches the UI to a running emulator core process.\n\n"

    "  the <console socket> parameter must be in the form [host:]port, where 'host' addresses the\n"
    "  machine on which the core process is running, and 'port' addresses the console port number for\n"
    "  the running core process. Note that 'host' value must be in the form that can be resolved\n"
    "  into an IP address.\n\n"

    "  Use -list-cores to enumerate console ports for all currently running core processes.\n"
    );
}
#endif  // CONFIG_STANDALONE_UI

static void
help_show_kernel(stralloc_t*  out)
{
    PRINTF(
    "  use '-show-kernel' to redirect debug messages from the kernel to the current\n"
    "  terminal. this is useful to check that the boot process works correctly.\n\n"
    );
}

static void
help_shell(stralloc_t*  out)
{
    PRINTF(
    "  use '-shell' to create a root shell console on the current terminal.\n"
    "  this is unlike the 'adb shell' command for the following reasons:\n\n"

    "  * this is a *root* shell that allows you to modify many parts of the system\n"
    "  * this works even if the ADB daemon in the emulated system is broken\n"
    "  * pressing Ctrl-C will stop the emulator, instead of the shell.\n\n"
    "  See also '-shell-serial'.\n\n" );
}

static void
help_shell_serial(stralloc_t*  out)
{
    PRINTF(
    "  use '-shell-serial <device>' instead of '-shell' to open a root shell\n"
    "  to the emulated system, while specifying an external communication\n"
    "  channel / host device.\n\n"

    "  '-shell-serial stdio' is identical to '-shell', while you can use\n"
    "  '-shell-serial tcp::4444,server,nowait' to talk to the shell over local\n"
    "  TCP port 4444.  '-shell-serial fdpair:3:6' would let a parent process\n"
    "  talk to the shell using fds 3 and 6.\n\n"

    "  see -help-char-devices for a list of available <device> specifications.\n\n"
    "  NOTE: you can have only one shell per emulator instance at the moment\n\n"
    );
}

static void
help_logcat(stralloc_t*  out)
{
    PRINTF(
    "  use '-logcat <tags>' to redirect log messages from the emulated system to\n"
    "  the current terminal. <tags> is a list of space/comma-separated log filters\n"
    "  where each filter has the following format:\n\n"

    "     <componentName>:<logLevel>\n\n"

    "  where <componentName> is either '*' or the name of a given component,\n"
    "  and <logLevel> is one of the following letters:\n\n"

    "      v          verbose level\n"
    "      d          debug level\n"
    "      i          informative log level\n"
    "      w          warning log level\n"
    "      e          error log level\n"
    "      s          silent log level\n\n"

    "  for example, the following only displays messages from the 'GSM' component\n"
    "  that are at least at the informative level:\n\n"

    "    -logcat '*:s GSM:i'\n\n"

    "  if '-logcat <tags>' is not used, the emulator looks for ANDROID_LOG_TAGS\n"
    "  in the environment. if it is defined, its value must match the <tags>\n"
    "  format and will be used to redirect log messages to the terminal.\n\n"

    "  note that this doesn't prevent you from redirecting the same, or other,\n"
    "  log messages through the ADB or DDMS tools too.\n\n");
}

static void
help_no_audio(stralloc_t*  out)
{
    PRINTF(
    "  use '-no-audio' to disable all audio support in the emulator. this may be\n"
    "  unfortunately be necessary in some cases:\n\n"

    "  * at least two users have reported that their Windows machine rebooted\n"
    "    instantly unless they used this option when starting the emulator.\n"
    "    it is very likely that the problem comes from buggy audio drivers.\n\n"

    "  * on some Linux machines, the emulator might get stuck at startup with\n"
    "    audio support enabled. this problem is hard to reproduce, but seems to\n"
    "    be related too to flaky ALSA / audio driver support.\n\n"

    "  on Linux, another option is to try to change the default audio backend\n"
    "  used by the emulator. you can do that by setting the QEMU_AUDIO_DRV\n"
    "  environment variables to one of the following values:\n\n"

    "    alsa        (use the ALSA backend)\n"
    "    esd         (use the EsounD backend)\n"
    "    sdl         (use the SDL audio backend, no audio input supported)\n"
    "    oss         (use the OSS backend)\n"
    "    none        (do not support audio)\n"
    "\n"
    "  the very brave can also try to use distinct backends for audio input\n"
    "  and audio outputs, this is possible by selecting one of the above values\n"
    "  into the QEMU_AUDIO_OUT_DRV and QEMU_AUDIO_IN_DRV environment variables.\n\n"
    );
}

static void
help_raw_keys(stralloc_t*  out)
{
    PRINTF(
    "  this option is deprecated because one can do the same using Ctrl-K\n"
    "  at runtime (this keypress toggles between unicode/raw keyboard modes)\n\n"

    "  by default, the emulator tries to reverse-map the characters you type on\n"
    "  your keyboard to device-specific key presses whenever possible. this is\n"
    "  done to make the emulator usable with a non-QWERTY keyboard.\n\n"

    "  however, this also means that single keypresses like Shift or Alt are not\n"
    "  passed to the emulated device. the '-raw-keys' option disables the reverse\n"
    "  mapping. it should only be used when using a QWERTY keyboard on your machine\n"

    "  (should only be useful to Android system hackers, e.g. when implementing a\n"
    "  new input method).\n\n"
    );
}

static void
help_radio(stralloc_t*  out)
{
    PRINTF(
    "  use '-radio <device>' to redirect the GSM modem emulation to an external\n"
    "  character device or program. this bypasses the emulator's internal modem\n"
    "  and should only be used for testing.\n\n"

    "  see '-help-char-devices' for the format of <device>\n\n"

    "  the data exchanged with the external device/program are GSM AT commands\n\n"

    "  note that, when running in the emulator, the Android GSM stack only supports\n"
    "  a *very* basic subset of the GSM protocol. trying to link the emulator to\n"
    "  a real GSM modem is very likely to not work properly.\n\n"
    );
}


static void
help_port(stralloc_t*  out)
{
    PRINTF(
    "  at startup, the emulator tries to bind its control console at a free port\n"
    "  starting from 5554, in increments of two (i.e. 5554, then 5556, 5558, etc..)\n"
    "  this allows several emulator instances to run concurrently on the same\n"
    "  machine, each one using a different console port number.\n\n"

    "  use '-port <port>' to force an emulator instance to use a given console port\n\n"

    "  note that <port> must be an *even* integer between 5554 and 5584 included.\n"
    "  <port>+1 must also be free and will be reserved for ADB. if any of these\n"
    "  ports is already used, the emulator will fail to start.\n\n" );
}

static void
help_ports(stralloc_t*  out)
{
    PRINTF(
    "  the '-ports <consoleport>,<adbport>' option allows you to explicitely set\n"
    "  the TCP ports used by the emulator to implement its control console and\n"
    "  communicate with the ADB tool.\n\n"

    "  This is a very special option that should probably *not* be used by typical\n"
    "  developers using the Android SDK (use '-port <port>' instead), because the\n"
    "  corresponding instance is probably not going to be seen from adb/DDMS. Its\n"
    "  purpose is to use the emulator in very specific network configurations.\n\n"

    "    <consoleport> is the TCP port used to bind the control console\n"
    "    <adbport> is the TCP port used to bind the ADB local transport/tunnel.\n\n"

    "  If both ports aren't available on startup, the emulator will exit.\n\n");
}


static void
help_onion(stralloc_t*  out)
{
    PRINTF(
    "  use '-onion <file>' to specify a PNG image file that will be displayed on\n"
    "  top of the emulated framebuffer with translucency. this can be useful to\n"
    "  check that UI elements are correctly positioned with regards to a reference\n"
    "  graphics specification.\n\n"

    "  the default translucency is 50%%, but you can use '-onion-alpha <%%age>' to\n"
    "  select a different one, or even use keypresses at runtime to alter it\n"
    "  (see -help-keys for details)\n\n"

    "  finally, the onion image can be rotated (see -help-onion-rotate)\n\n"
    );
}

static void
help_onion_alpha(stralloc_t*  out)
{
    PRINTF(
    "  use '-onion-alpha <percent>' to change the translucency level of the onion\n"
    "  image that is going to be displayed on top of the framebuffer (see also\n"
    "  -help-onion). the default is 50%%.\n\n"

    "  <percent> must be an integer between 0 and 100.\n\n"

    "  you can also change the translucency dynamically (see -help-keys)\n\n"
    );
}

static void
help_onion_rotation(stralloc_t*  out)
{
    PRINTF(
    "  use '-onion-rotation <rotation>' to change the rotation of the onion\n"
    "  image loaded through '-onion <file>'. valid values for <rotation> are:\n\n"

    "   0        no rotation\n"
    "   1        90  degrees clockwise\n"
    "   2        180 degrees\n"
    "   3        270 degrees clockwise\n\n"
    );
}


static void
help_timezone(stralloc_t*  out)
{
    PRINTF(
    "  by default, the emulator tries to detect your current timezone to report\n"
    "  it to the emulated system. use the '-timezone <timezone>' option to choose\n"
    "  a different timezone, or if the automatic detection doesn't work correctly.\n\n"

    "  VERY IMPORTANT NOTE:\n\n"
    "  the <timezone> value must be in zoneinfo format, i.e. it should look like\n"
    "  Area/Location or even Area/SubArea/Location. valid examples are:\n\n"

    "    America/Los_Angeles\n"
    "    Europe/Paris\n\n"

    "  using a human-friendly abbreviation like 'PST' or 'CET' will not work, as\n"
    "  well as using values that are not defined by the zoneinfo database.\n\n"

    "  NOTE: unfortunately, this will not work on M5 and older SDK releases\n\n"
    );
}


static void
help_dns_server(stralloc_t*  out)
{
    PRINTF(
    "  by default, the emulator tries to detect the DNS servers you're using and\n"
    "  will setup special aliases in the emulated firewall network to allow the\n"
    "  Android system to connect directly to them. use '-dns-server <servers>' to\n"
    "  select a different list of DNS servers to be used.\n\n"

    "  <servers> must be a comma-separated list of up to 4 DNS server names or\n"
    "  IP addresses.\n\n"

    "  NOTE: on M5 and older SDK releases, only the first server in the list will\n"
    "        be used.\n\n"
    );
}


static void
help_cpu_delay(stralloc_t*  out)
{
    PRINTF(
    "  this option is purely experimental, probably doesn't work as you would\n"
    "  expect, and may even disappear in a later emulator release.\n\n"

    "  use '-cpu-delay <delay>' to throttle CPU emulation. this may be useful\n"
    "  to detect weird race conditions that only happen on 'lower' CPUs. note\n"
    "  that <delay> is a unit-less integer that doesn't even scale linearly\n"
    "  to observable slowdowns. use trial and error to find something that\n"
    "  suits you, the 'correct' machine is very probably dependent on your\n"
    "  host CPU and memory anyway...\n\n"
    );
}


static void
help_no_boot_anim(stralloc_t*  out)
{
    PRINTF(
    "  use '-no-boot-anim' to disable the boot animation (red bouncing ball) when\n"
    "  starting the emulator. on slow machines, this can surprisingly speed up the\n"
    "  boot sequence in tremendous ways.\n\n"

    "  NOTE: unfortunately, this will not work on M5 and older SDK releases\n\n"
    );
}


static void
help_gps(stralloc_t*  out)
{
    PRINTF(
    "  use '-gps <device>' to emulate an NMEA-compatible GPS unit connected to\n"
    "  an external character device or socket. the format of <device> is the same\n"
    "  than the one used for '-radio <device>' (see -help-char-devices for details)\n\n"
    );
}


static void
help_keyset(stralloc_t*  out)
{
    char  temp[256];

    PRINTF(
    "  use '-keyset <name>' to specify a different keyset file name to use when\n"
    "  starting the emulator. a keyset file contains a list of key bindings used\n"
    "  to control the emulator with the host keyboard.\n\n"

    "  by default, the emulator looks for the following file:\n\n"
    );

    bufprint_config_file(temp, temp+sizeof(temp), KEYSET_FILE);
    PRINTF(
    "    %s\n\n", temp );

    bufprint_config_path(temp, temp+sizeof(temp));
    PRINTF(
    "  however, if -keyset is used, then the emulator does the following:\n\n"

    "  - first, if <name> doesn't have an extension, then the '.keyset' suffix\n"
    "    is appended to it (e.g. \"foo\" => \"foo.keyset\"),\n\n"

    "  - then, the emulator searches for a file named <name> in the following\n"
    "    directories:\n\n"

    "     * the emulator configuration directory: %s\n"
    "     * the 'keysets' subdirectory of <systemdir>, if any\n"
    "     * the 'keysets' subdirectory of the program location, if any\n\n",
    temp );

    PRINTF(
    "  if no corresponding file is found, a default set of key bindings is used.\n\n"
    "  use '-help-keys' to list the default key bindings.\n"
    "  use '-help-keyset-file' to learn more about the format of keyset files.\n"
    "\n"
    );
}

#ifdef CONFIG_NAND_LIMITS
static void
help_nand_limits(stralloc_t*  out)
{
    PRINTF(
    "  use '-nand-limits <limits>' to enable a debugging feature that sends a\n"
    "  signal to an external process once a read and/or write limit is achieved\n"
    "  in the emulated system. the format of <limits> is the following:\n\n"

    "     pid=<number>,signal=<number>,[reads=<threshold>][,writes=<threshold>]\n\n"

    "  where 'pid' is the target process identifier, 'signal' the number of the\n"
    "  target signal. the read and/or write threshold'reads' are a number optionally\n"
    "  followed by a K, M or G suffix, corresponding to the number of bytes to be\n"
    "  read or written before the signal is sent.\n\n"
    );
}
#endif /* CONFIG_NAND_LIMITS */

static void
help_bootchart(stralloc_t  *out)
{
    PRINTF(
    "  some Android system images have a modified 'init' system that  integrates\n"
    "  a bootcharting facility (see http://www.bootchart.org/). You can pass a\n"
    "  bootcharting period to the system with the following:\n\n"

    "    -bootchart <timeout>\n\n"

    "  where 'timeout' is a period expressed in seconds. Note that this won't do\n"
    "  anything if your init doesn't have bootcharting activated.\n\n"
    );
}

static void
help_tcpdump(stralloc_t  *out)
{
    PRINTF(
    "  use the -tcpdump <file> option to start capturing all network packets\n"
    "  that are sent through the emulator's virtual Ethernet LAN. You can later\n"
    "  use tools like WireShark to analyze the traffic and understand what\n"
    "  really happens.\n\n"

    "  note that this captures all Ethernet packets, and is not limited to TCP\n"
    "  connections.\n\n"

    "  you can also start/stop the packet capture dynamically through the console;\n"
    "  see the 'network capture start' and 'network capture stop' commands for\n"
    "  details.\n\n"
    );
}

static void
help_charmap(stralloc_t  *out)
{
    PRINTF(
    "  use '-charmap <file>' to use key character map specified in that file.\n"
    "  <file> must be a full path to a kcm file, containing desired character map.\n\n"
    );
}

static void
help_prop(stralloc_t  *out)
{
    PRINTF(
    "  use '-prop <name>=<value>' to set a boot-time system property.\n"
    "  <name> must be a property name of at most %d characters, without any\n"
    "  space in it, and <value> must be a string of at most %d characters.\n\n",
    BOOT_PROPERTY_MAX_NAME, BOOT_PROPERTY_MAX_VALUE );

    PRINTF(
    "  the corresponding system property will be set at boot time in the\n"
    "  emulated system. This can be useful for debugging purposes.\n\n"

    "  note that you can use several -prop options to define more than one\n"
    "  boot property.\n\n"
    );
}

static void
help_shared_net_id(stralloc_t*  out)
{
    PRINTF(
    "  Normally, Android instances running in the emulator cannot talk to each other\n"
    "  directly, because each instance is behind a virtual router. However, sometimes\n"
    "  it is necessary to test the behaviour of applications if they are directly\n"
    "  exposed to the network.\n\n"

    "  This option instructs the emulator to join a virtual network shared with\n"
    "  emulators also using this option. The number given is used to construct\n"
    "  the IP address 10.1.2.<number>, which is bound to a second interface on\n"
    "  the emulator. Each emulator must use a different number.\n\n"
    );
}

static void
help_gpu(stralloc_t* out)
{
    PRINTF(
    "  Use -gpu <mode> to override the mode of hardware OpenGL ES emulation\n"
    "  indicated by the AVD. Valid values for <mode> are:\n\n"

    "     on       -> enable GPU emulation\n"
    "     off      -> disable GPU emulation\n"
    "     auto     -> use the setting from the AVD\n"
    "     enabled  -> same as 'on'\n"
    "     disabled -> same as 'off'\n\n"

    "  Note that enabling GPU emulation if the system image does not support it\n"
    "  will prevent the proper display of the emulated framebuffer.\n\n"

    "  You can always disable GPU emulation (i.e. '-gpu off'), and this will\n"
    "  force the virtual device to use the slow software renderer instead.\n"
    "  Note that OpenGLES 2.0 is _not_ supported by it.\n\n"

    "  The 'auto' mode is the default. In this mode, the hw.gpu.enabled setting\n"
    "  in the AVD's hardware-qemu.ini file will determine whether GPU emulation\n"
    "  is enabled.\n\n"

    "  Even if hardware GPU emulation is enabled, if the host-side OpenGL ES\n"
    "  emulation library cannot be initialized, the emulator will run with GPU\n"
    "  emulation disabled rather than failing to start.\n"
    );
}

static void
help_camera_back(stralloc_t* out)
{
    PRINTF(
    "  Use -camera-back <mode> to control emulation of a camera facing back.\n"
    "  Valid values for <mode> are:\n\n"

    "     emulated  -> camera will be emulated using software ('fake') camera emulation\n"
    "     webcam<N> -> camera will be emulated using a webcamera connected to the host\n"
    "     none      -> camera emulation will be disabled\n\n"
    );
}

static void
help_camera_front(stralloc_t* out)
{
    PRINTF(
    "  Use -camera-front <mode> to control emulation of a camera facing front.\n"
    "  Valid values for <mode> are:\n\n"

    "     emulated  -> camera will be emulated using software ('fake') camera emulation\n"
    "     webcam<N> -> camera will be emulated using a webcamera connected to the host\n"
    "     none      -> camera emulation will be disabled\n\n"
    );
}

static void
help_webcam_list(stralloc_t* out)
{
    PRINTF(
    "  Use -webcam-list to list web cameras available for emulation.\n\n"
    );
}

static void
help_screen(stralloc_t* out)
{
    PRINTF(
    "  Use -screen <mode> to set the emulated screen mode.\n"
    "  Valid values for <mode> are:\n\n"

    "     touch       -> emulate a touch screen\n"
    "     multi-touch -> emulate a multi-touch screen\n"
    "     no-touch    -> disable touch and multi-touch screen emulation\n\n"

    "  Default mode for screen emulation is 'touch'.\n\n"
    );
}

static void
help_force_32bit(stralloc_t* out)
{
    PRINTF(
    "  Use -force-32bit to use 32-bit emulator on 64-bit platforms\n\n"

    );
}

#define  help_no_skin   NULL
#define  help_netspeed  help_shaper
#define  help_netdelay  help_shaper
#define  help_netfast   help_shaper

#define  help_noaudio      NULL
#define  help_noskin       NULL
#define  help_nocache      NULL
#define  help_no_jni       NULL
#define  help_nojni        NULL
#define  help_initdata     NULL
#define  help_no_window    NULL
#define  help_version      NULL
#define  help_memory       NULL
#define  help_partition_size NULL

typedef struct {
    const char*  name;
    const char*  template;
    const char*  descr;
    void (*func)(stralloc_t*);
} OptionHelp;

static const OptionHelp    option_help[] = {
#define  OPT_FLAG(_name,_descr)             { STRINGIFY(_name), NULL, _descr, help_##_name },
#define  OPT_PARAM(_name,_template,_descr)  { STRINGIFY(_name), _template, _descr, help_##_name },
#define  OPT_LIST                           OPT_PARAM
#include "android/cmdline-options.h"
    { NULL, NULL, NULL, NULL }
};

typedef struct {
    const char*  name;
    const char*  desc;
    void (*func)(stralloc_t*);
} TopicHelp;


static const TopicHelp    topic_help[] = {
    { "disk-images",  "about disk images",      help_disk_images },
    { "keys",         "supported key bindings", help_keys },
    { "debug-tags",   "debug tags for -debug <tags>", help_debug_tags },
    { "char-devices", "character <device> specification", help_char_devices },
    { "environment",  "environment variables",  help_environment },
    { "keyset-file",  "key bindings configuration file", help_keyset_file },
    { "virtual-device", "virtual device management", help_virtual_device },
    { "sdk-images",   "about disk images when using the SDK", help_sdk_images },
    { "build-images", "about disk images when building Android", help_build_images },
    { NULL, NULL, NULL }
};

int
android_help_for_option( const char*  option, stralloc_t*  out )
{
    OptionHelp const*  oo;
    char               temp[32];

    /* the names in the option_help table use underscore instead
     * of dashes, so create a tranlated copy of the option name
     * before scanning the table for matches
     */
    buffer_translate_char( temp, sizeof temp, option, '-', '_' );

    for ( oo = option_help; oo->name != NULL; oo++ ) {
        if ( !strcmp(oo->name, temp) ) {
            if (oo->func)
                oo->func(out);
            else
                stralloc_add_str(out, oo->descr);
            return 0;
        }
    }
    return -1;
}


int
android_help_for_topic( const char*  topic, stralloc_t*  out )
{
    const TopicHelp*  tt;

    for ( tt = topic_help; tt->name != NULL; tt++ ) {
        if ( !strcmp(tt->name, topic) ) {
            tt->func(out);
            return 0;
        }
    }
    return -1;
}


extern void
android_help_list_options( stralloc_t*  out )
{
    const OptionHelp*  oo;
    const TopicHelp*   tt;
    int                maxwidth = 0;

    for ( oo = option_help; oo->name != NULL; oo++ ) {
        int  width = strlen(oo->name);
        if (oo->template != NULL)
            width += strlen(oo->template);
        if (width > maxwidth)
            maxwidth = width;
    }

    for (oo = option_help; oo->name != NULL; oo++) {
        char  temp[32];
        /* the names in the option_help table use underscores instead
         * of dashes, so create a translated copy of the option's name
         */
        buffer_translate_char(temp, sizeof temp, oo->name, '_', '-');

        stralloc_add_format( out, "    -%s %-*s %s\n",
            temp,
            (int)(maxwidth - strlen(oo->name)),
            oo->template ? oo->template : "",
            oo->descr );
    }

    PRINTF( "\n" );
    PRINTF( "     %-*s  %s\n", maxwidth, "-qemu args...",    "pass arguments to qemu");
    PRINTF( "     %-*s  %s\n", maxwidth, "-qemu -h", "display qemu help");
    PRINTF( "\n" );
    PRINTF( "     %-*s  %s\n", maxwidth, "-verbose",       "same as '-debug-init'");
    PRINTF( "     %-*s  %s\n", maxwidth, "-debug <tags>",  "enable/disable debug messages");
    PRINTF( "     %-*s  %s\n", maxwidth, "-debug-<tag>",   "enable specific debug messages");
    PRINTF( "     %-*s  %s\n", maxwidth, "-debug-no-<tag>","disable specific debug messages");
    PRINTF( "\n" );
    PRINTF( "     %-*s  %s\n", maxwidth, "-help",    "print this help");
    PRINTF( "     %-*s  %s\n", maxwidth, "-help-<option>", "print option-specific help");
    PRINTF( "\n" );

    for (tt = topic_help; tt->name != NULL; tt += 1) {
        char    help[32];
        snprintf(help, sizeof(help), "-help-%s", tt->name);
        PRINTF( "     %-*s  %s\n", maxwidth, help, tt->desc );
    }
    PRINTF( "     %-*s  %s\n", maxwidth, "-help-all", "prints all help content");
    PRINTF( "\n");
}


void
android_help_main( stralloc_t*  out )
{
    stralloc_add_str(out, "Android Emulator usage: emulator [options] [-qemu args]\n");
    stralloc_add_str(out, "  options:\n" );

    android_help_list_options(out);

    /*printf( "%.*s", out->n, out->s );*/
}


void
android_help_all( stralloc_t*  out )
{
    const OptionHelp*  oo;
    const TopicHelp*   tt;

    for (oo = option_help; oo->name != NULL; oo++) {
        PRINTF( "========= help for option -%s:\n\n", oo->name );
        android_help_for_option( oo->name, out );
    }

    for (tt = topic_help; tt->name != NULL; tt++) {
        PRINTF( "========= help for -help-%s\n\n", tt->name );
        android_help_for_topic( tt->name, out );
    }
    PRINTF( "========= top-level help\n\n" );
    android_help_main(out);
}
