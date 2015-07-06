/* this header file can be included several times by the same source code.
 * it contains the list of support command-line options for the Android
 * emulator program
 */
#ifndef OPT_PARAM
#error OPT_PARAM is not defined
#endif
#ifndef OPT_LIST
#error OPT_LIST is not defined
#endif
#ifndef OPT_FLAG
#error OPT_FLAG is not defined
#endif
#ifndef CFG_PARAM
#define CFG_PARAM  OPT_PARAM
#endif
#ifndef CFG_FLAG
#define CFG_FLAG   OPT_FLAG
#endif

/* required to ensure that the CONFIG_XXX macros are properly defined */
#include "android/config/config.h"

/* Some options acts like flags, while others must be followed by a parameter
 * string. Nothing really new here.
 *
 * Some options correspond to AVD (Android Virtual Device) configuration
 * and will be ignored if you start the emulator with the -avd <name> flag.
 *
 * However, if you use them with -avd-create <name>, these options will be
 * recorded into the new AVD directory. Once an AVD is created, there is no
 * way to change these options.
 *
 * Several macros are used to define options:
 *
 *    OPT_FLAG( name, "description" )
 *       used to define a non-config flag option.
 *       * 'name' is the option suffix that must follow the dash (-)
 *          as well as the name of an integer variable whose value will
 *          be 1 if the flag is used, or 0 otherwise.
 *       * "description" is a short description string that will be
 *         displayed by 'emulator -help'.
 *
 *    OPT_PARAM( name, "<param>", "description" )
 *       used to define a non-config parameter option
 *        * 'name' will point to a char* variable (NULL if option is unused)
 *        * "<param>" is a template for the parameter displayed by the help
 *        * 'varname' is the name of a char* variable that will point
 *          to the parameter string, if any, or will be NULL otherwise.
 *
 *    CFG_FLAG( name, "description" )
 *        used to define a configuration-specific flag option.
 *
 *    CFG_PARAM( name, "<param>", "description" )
 *        used to define a configuration-specific parameter option.
 *
 * NOTE: Keep in mind that optio names are converted by translating
 *       dashes into underscore.
 *
 *       This means that '-some-option' is equivalent to '-some_option'
 *       and will be backed by a variable name 'some_option'
 *
 */

CFG_PARAM( sysdir,  "<dir>",  "search for system disk images in <dir>" )
CFG_PARAM( system,  "<file>", "read initial system image from <file>" )
CFG_PARAM( datadir, "<dir>",  "write user data into <dir>" )
CFG_PARAM( kernel,  "<file>", "use specific emulated kernel" )
CFG_PARAM( ramdisk, "<file>", "ramdisk image (default <system>/ramdisk.img" )
CFG_PARAM( image,   "<file>", "obsolete, use -system <file> instead" )
CFG_PARAM( initdata, "<file>", "same as '-init-data <file>'" )
CFG_PARAM( data,     "<file>", "data image (default <datadir>/userdata-qemu.img" )
CFG_PARAM( partition_size, "<size>", "system/data partition size in MBs" )
CFG_PARAM( cache,    "<file>", "cache partition image (default is temporary file)" )
OPT_PARAM( cache_size, "<size>", "cache partition size in MBs" )
CFG_FLAG ( no_cache, "disable the cache partition" )
CFG_FLAG ( nocache,  "same as -no-cache" )
OPT_PARAM( sdcard, "<file>", "SD card image (default <system>/sdcard.img")
OPT_PARAM( snapstorage,    "<file>", "file that contains all state snapshots (default <datadir>/snapshots.img)")
OPT_FLAG ( no_snapstorage, "do not mount a snapshot storage file (this disables all snapshot functionality)" )
OPT_PARAM( snapshot,       "<name>", "name of snapshot within storage file for auto-start and auto-save (default 'default-boot')" )
OPT_FLAG ( no_snapshot,    "perform a full boot and do not do not auto-save, but qemu vmload and vmsave operate on snapstorage" )
OPT_FLAG ( no_snapshot_save, "do not auto-save to snapshot on exit: abandon changed state" )
OPT_FLAG ( no_snapshot_load, "do not auto-start from snapshot: perform a full boot" )
OPT_FLAG ( snapshot_list,  "show a list of available snapshots" )
OPT_FLAG ( no_snapshot_update_time, "do not do try to correct snapshot time on restore" )
OPT_FLAG ( wipe_data, "reset the user data image (copy it from initdata)" )
CFG_PARAM( avd, "<name>", "use a specific android virtual device" )
CFG_PARAM( skindir, "<dir>", "search skins in <dir> (default <system>/skins)" )
CFG_PARAM( skin, "<name>", "select a given skin" )
CFG_FLAG ( no_skin, "don't use any emulator skin" )
CFG_FLAG ( noskin, "same as -no-skin" )
CFG_FLAG ( dynamic_skin, "dynamically construct a skin of given size, requires -skin WxH option" )
CFG_PARAM( memory, "<size>", "physical RAM size in MBs" )

OPT_PARAM( netspeed, "<speed>", "maximum network download/upload speeds" )
OPT_PARAM( netdelay, "<delay>", "network latency emulation" )
OPT_FLAG ( netfast, "disable network shaping" )

OPT_PARAM( trace, "<name>", "enable code profiling (F9 to start)" )
OPT_FLAG ( show_kernel, "display kernel messages" )
OPT_FLAG ( shell, "enable root shell on current terminal" )
OPT_FLAG ( no_jni, "disable JNI checks in the Dalvik runtime" )
OPT_FLAG ( nojni, "same as -no-jni" )
OPT_PARAM( logcat, "<tags>", "enable logcat output with given tags" )

OPT_FLAG ( no_audio, "disable audio support" )
OPT_FLAG ( noaudio,  "same as -no-audio" )
OPT_PARAM( audio,    "<backend>", "use specific audio backend" )

OPT_FLAG ( raw_keys, "disable Unicode keyboard reverse-mapping" )
OPT_PARAM( radio, "<device>", "redirect radio modem interface to character device" )
OPT_PARAM( port, "<port>", "TCP port that will be used for the console" )
OPT_PARAM( ports, "<consoleport>,<adbport>", "TCP ports used for the console and adb bridge" )
OPT_PARAM( onion, "<image>", "use overlay PNG image over screen" )
OPT_PARAM( onion_alpha, "<%age>", "specify onion-skin translucency" )
OPT_PARAM( onion_rotation, "0|1|2|3", "specify onion-skin rotation" )

OPT_PARAM( scale, "<scale>", "scale emulator window" )
OPT_PARAM( dpi_device, "<dpi>", "specify device's resolution in dpi (default "
            STRINGIFY(DEFAULT_DEVICE_DPI) ")" )

OPT_PARAM( http_proxy, "<proxy>", "make TCP connections through a HTTP/HTTPS proxy" )
OPT_PARAM( timezone, "<timezone>", "use this timezone instead of the host's default" )
OPT_PARAM( dns_server, "<servers>", "use this DNS server(s) in the emulated system" )
OPT_PARAM( cpu_delay, "<cpudelay>", "throttle CPU emulation" )
OPT_FLAG ( no_boot_anim, "disable animation for faster boot" )

OPT_FLAG( no_window, "disable graphical window display" )
OPT_FLAG( version, "display emulator version number" )

OPT_PARAM( report_console, "<socket>", "report console port to remote socket" )
OPT_PARAM( gps, "<device>", "redirect NMEA GPS to character device" )
OPT_PARAM( keyset, "<name>", "specify keyset file name" )
OPT_PARAM( shell_serial, "<device>", "specific character device for root shell" )
OPT_PARAM( tcpdump, "<file>", "capture network packets to file" )

OPT_PARAM( bootchart, "<timeout>", "enable bootcharting")

OPT_PARAM( charmap, "<file>", "use specific key character map")

OPT_LIST(  prop, "<name>=<value>", "set system property on boot")

OPT_PARAM( shared_net_id, "<number>", "join the shared network, using IP address 10.1.2.<number>")

#ifdef CONFIG_NAND_LIMITS
OPT_PARAM( nand_limits, "<nlimits>", "enforce NAND/Flash read/write thresholds" )
#endif

#ifdef CONFIG_MEMCHECK
OPT_PARAM( memcheck, "<flags>", "enable memory access checking" )
#endif  // CONFIG_MEMCHECK

#ifdef CONFIG_STANDALONE_UI
OPT_PARAM( list_cores, "<host>", "list running core process" )
OPT_PARAM( attach_core, "<console socket>", "attach to a running core process" )
#endif  // CONFIG_STANDALONE_UI

OPT_PARAM( gpu, "<mode>", "set hardware OpenGLES emulation mode" )

OPT_PARAM( camera_back, "<mode>", "set emulation mode for a camera facing back" )
OPT_PARAM( camera_front, "<mode>", "set emulation mode for a camera facing front" )
OPT_FLAG( webcam_list, "lists web cameras available for emulation" )

OPT_PARAM( screen, "<mode>", "set emulated screen mode" )

OPT_FLAG( force_32bit, "always use 32-bit emulator" )

#undef CFG_FLAG
#undef CFG_PARAM
#undef OPT_FLAG
#undef OPT_PARAM
#undef OPT_LIST
