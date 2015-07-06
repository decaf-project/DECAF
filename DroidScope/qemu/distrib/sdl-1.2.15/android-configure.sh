#/bin/sh
#
# this script is used to rebuild SDL from sources in the current
# directory. It depends on the Android emulator build scripts
# that should normally be located in ../emulator/android/build
# but you can also use ANDROID_QEMU_PATH in your environment
# to point to the top of the Android emulator sources
#
#

# first, let's see which system we're running this on
cd `dirname $0`

# try to find the Android build directory automatically
if [ -z "$ANDROID_QEMU_PATH" ] ; then
    if [ -f ../../android/build/common.sh ] ; then
        ANDROID_QEMU_PATH=../..
    else
        echo "You must define ANDROID_QEMU_PATH in your environment to point"
        echo "to the directory containing the Android emulator's sources."
        exit 1
    fi
fi

if [ ! -d $ANDROID_QEMU_PATH ] ; then
    echo "Your ANDROID_QEMU_PATH does not point to a valid directory."
    exit 1
fi

if [ ! -f $ANDROID_QEMU_PATH/android/build/common.sh ] ; then
    echo "Your ANDROID_QEMU_PATH does not point to a directory that"
    echo "contains the Android emulator build scripts."
    exit 1
fi

# source common functions definitions
. $ANDROID_QEMU_PATH/android/build/common.sh

# Parse options
OPTION_TRY_64=no
OPTION_HELP=no
OPTION_TARGETS=
OPTION_FORCE_AUTODETECT=no
OPTION_NO_TIGER=no

if [ -z "$CC" ] ; then
  CC=gcc
fi

for opt do
  optarg=`expr "x$opt" : 'x[^=]*=\(.*\)'`
  case "$opt" in
  --help|-h|-\?) OPTION_HELP=yes
  ;;
  --verbose)
    if [ "$VERBOSE" = "yes" ] ; then
        VERBOSE2=yes
    else
        VERBOSE=yes
    fi
  ;;
  --cc=*) CC="$optarg" ; HOSTCC=$CC
  ;;
  --try-64) OPTION_TRY_64=yes
  ;;
  --prefix=*) OPTION_TARGETS="$OPTION_TARGETS $optarg"
  ;;
  --force-autodetect) OPTION_FORCE_AUTODETECT=yes
  ;;
  --no-tiger) OPTION_NO_TIGER=yes
  ;;
  *)
    echo "unknown option '$opt', use --help"
    exit 1
  esac
done

# Print the help message
#
if [ "$OPTION_HELP" = "yes" ] ; then
    cat << EOF

Usage: rebuild.sh [options]
Options: [defaults in brackets after descriptions]
EOF
    echo "Standard options:"
    echo "  --help                   print this message"
    echo "  --cc=PATH                specify C compiler [$CC]"
    echo "  --try-64                 try to build a 64-bit executable (may crash)"
    echo "  --verbose                verbose configuration"
    echo "  --force-autodetect       force feature auto-detection"
    echo "  --no-tiger               do not generate Tiger-compatible binaries (OS X only)"
    echo ""
    exit 1
fi

# we only support generating 32-bit binaris on 64-bit systems.
# And we may need to add a -Wa,--32 to CFLAGS to let the assembler
# generate 32-bit binaries on Linux x86_64.
#
if [ "$OPTION_TRY_64" != "yes" ] ; then
    force_32bit_binaries
fi

# default target
if [ -z "$OPTION_TARGETS" ] ; then
  OPTION_TARGETS=out/$OS
fi

# we can build SDL with Cygwin, so enable it
enable_cygwin

setup_toolchain

###
### Special compiler and linker flags
###

case "$OS" in
    linux-*)
        BUILD_CFLAGS="-D_GNU_SOURCE=1 -fvisibility=hidden -DXTHREADS -D_REENTRANT"
        # prevent -fstack-protector or the generated binaries will not link
        BUILD_CFLAGS="$BUILD_CFLAGS -fno-stack-protector"
        ;;
    darwin-*)
        BUILD_CFLAGS="-D_GNU_SOURCE=1 -fvisibility=hidden -DTARGET_API_MAC_CARBON -DTARGET_API_MAC_OSX -D_THREAD_SAFE -force_cpusubtype_ALL -fpascal-strings"
        # detect the 10.4 SDK and use it automatically
        TIGER_SDK=/Developer/SDKs/MacOSX10.4u.sdk
        if [ ! -d $TIGER_SDK -a $OPTION_NO_TIGER = no ] ; then
            echo "Please install the 10.4 SDK at $TIGER_SDK to generate Tiger-compatible binaries."
            echo "If you don't want compatibility, use --no-tiger option instead."
            exit 1
        fi
        if [ -d $TIGER_SDK ] ; then
            TIGER_OPTS="-isysroot $TIGER_SDK -mmacosx-version-min=10.4"
            BUILD_CFLAGS="$BUILD_CFLAGS $TIGER_OPTS"
            BUILD_LDFLAGS="$BUILD_LDFLAGS $TIGER_OPTS -Wl,-syslibroot,$(TIGER_SDK)"
            echo "Using OS X 10.4 SDK to generate Tiger-compatible binaries."
        else
            echo "Warning: the generated binaries will not be compatible with Tiger."
        fi
        ;;
    windows)
        BUILD_CFLAGS="-D_GNU_SOURCE=1"
        ;;
    *)
        BUILD_CFLAGS=
esac

# BUILD_CFLAGS are used to build SDL, but SDL_CFLAGS are used
# when clients want to use its facilities
#
case "$HOST_OS" in
    linux)
        SDL_CFLAGS="-D_GNU_SOURCE=1 -D_REENTRANT"
        SDL_STATIC_LIBS="-lm -ldl -lpthread -lrt"
        ;;
    darwin)
        SDL_CFLAGS="-D_GNU_SOURCE=1 -D_THREAD_SAFE"
        SDL_STATIC_LIBS="-Wl,-framework,OpenGL -Wl,-framework,Cocoa -Wl,-framework,QuickTime -Wl,-framework,ApplicationServices -Wl,-framework,Carbon -Wl,-framework,IOKit"
        ;;
    windows)
        SDL_CFLAGS="-D_GNU_SOURCE=1 -Dmain=SDL_main"
        SDL_STATIC_LIBS="-luser32 -lgdi32 -lwinmm"
        ;;
    *)
        SDL_CFLAGS=
        SDL_STATIC_LIBS=
esac

DOLLAR="$"
SDL_STATIC_LIBS="$DOLLAR{libdir}/libSDLmain.a $DOLLAR{libdir}/libSDL.a $SDL_STATIC_LIBS"

###
### Features probes
###

CONFIG_LIBC=yes

###
### Configuration files generation
###

# create the objs directory that is going to contain all generated files
# including the configuration ones
#
mkdir -p objs

config_add ()
{
    echo "$1" >> $config_h
}

# used to add a macro define/undef line to the configuration
# $1: macro name
# $2: 1, 0, yes or no
# $3: optional log prefix
config_add_macro ()
{
    local logname
    logname="${3:-CfgMacro   }"
    case "$2" in
        0|no|false)
            config_add "/* #undef $1 */"
            log "$logname: $1=0"
            ;;
        1|yes|true)
            config_add "#define $1 1"
            log "$logname: $1=1"
            ;;
    esac
}

# used to add host-os-specific driver macros to the config file
# $1 : driver prefix, e.g. DRIVERS_VIDEO
# $2 : macro prefix
# this will look for DRIVERS_VIDEO_${HOST_OS}, and, if it is not
# defined in DRIVERS_VIDEO_default
#
# then this will call 'config_add ${2}_${driver}' for each driver listed
#
config_add_driver_macros ()
{
    local  driver_list="`var_value ${1}_${HOST_OS}`"
    local  driver_name
    if [ -z "$driver_list" ] ; then
        driver_list="`var_value ${1}_default`"
    fi
    for driver_name in $driver_list; do
        config_add_macro "${2}_${driver_name}" yes
    done
}

config_h=objs/SDL_config.h

# a function used to check the availability of a given Standard C header
# $1 : header name, without the .h suffix nor enclosing brackets (e.g. 'memory' for <memory.h>)
# this will update the configuration with a relevant line
sdl_check_header ()
{
    local result
    local header
    local macro
    header="<$1.h>"
    macro=`to_uppercase $1`
    macro=`echo $macro | tr '/' '_'`
    feature_check_header result "$header"
    config_add_macro HAVE_${macro}_H $result "StdCHeader "
}

# call sdl_check_header with a list of header names
sdl_check_headers ()
{
    for hh in $*; do
        sdl_check_header "$hh"
    done
}

# a function used to check that a given function is available in the C library
# this will update the configuration with a relevant line
# $1: function name
# $2: optional, libraries to link against
#
sdl_check_func ()
{
    local result
    local funcname
    local macro
    funcname="$1"
    macro=`to_uppercase $1`
    rm -f $TMPC
    cat > $TMPC <<EOF
#undef $funcname
#define $funcname  _innocuous_$funcname
#include <assert.h>
#undef $funcname

typedef void (*func_t)(void);
extern void $funcname(void);
func_t  _dummy = $funcname;
int   main(void)
{
    return _dummy == $funcname;
}
EOF
    EXTRA_LDFLAGS="$2"
    feature_check_link result
    config_add_macro HAVE_${macro} $result "StdCFunc   "
}

# call sdl_check_func with a list of functions
sdl_check_funcs ()
{
    for ff in $*; do
        sdl_check_func "$ff"
    done
}

# check endianess of the host platform
sdl_check_endianess ()
{
    cat > $TMPC <<EOF
    int  main(void)
    {
        union {
            unsigned short  v;
            unsigned char   b[2];
        } u;

        u.v = 0x1234;
        return (u.b[0] == 0x12);
    }
EOF
    feature_run_exec test_e
    if [ "$test_e" = "1" ] ; then
        eval $1=4321
    else
        eval $1=1234
    fi
}


generate_SDL_config_h ()
{
    echo "/* This file was autogenerated by '$PROGNAME' - do not edit */" > $config_h

    config_add "#ifndef _SDL_config_h"
    config_add "#define _SDL_config_h"
    config_add ""
    config_add "#include \"SDL_platform.h\""
    config_add ""
    
    # true for all architectures these days
    config_add_macro SDL_HAS_64BIT_TYPE  1

    sdl_check_endianess ENDIANESS
    config_add "#define SDL_BYTEORDER $ENDIANESS"
    config_add ""

    config_add_macro HAVE_LIBC $CONFIG_LIBC

    config_add "#if HAVE_LIBC"
    config_add ""
    config_add "/* Useful headers */"

    sdl_check_headers  alloca sys/types stdio

    # just like configure - force it to 1
    config_add "#define STDC_HEADERS  1"

    sdl_check_headers  stdlib stdarg malloc memory string strings inttypes stdint ctype math iconv signal altivec

    config_add "/* C library functions */"

    sdl_check_funcs  malloc calloc realloc free alloc

    config_add "#ifndef _WIN32 /* Don't use on Windows */"

    sdl_check_funcs  getenv putenv unsetenv

    config_add "#endif"
    sdl_check_funcs qsort abs bcopy memset memcpy memmove memcmp strlen strlcpy strlcat
    sdl_check_funcs strdup _strrev _strupr _strlwr index rindex strchr strrchr itoa _ltoa
    sdl_check_funcs _uitoa _ultoa strtol strtoul _i64toa _ui64toa strtoll strtoull
    sdl_check_funcs strtod atoi atof strcmp strncmp _stricmp strcasecmp _strnicmp
    sdl_check_funcs vsnprintf iconv sigaction setjmp nanosleep

    sdl_check_func clock_gettime -lrt
    sdl_check_func dlvsym -ldl

    sdl_check_funcs getpagesize

    config_add "#else"
    config_add "/* We may need some replacement for stdarg.h here */"
    config_add "#include <stdarg.h>"
    config_add "#endif /* HAVE_LIBC */"
    config_add ""

    config_add "/* Allow disabling of core subsystems */"

    config_add_macro SDL_AUDIO_DISABLED yes
    config_add_macro SDL_CDROM_DISABLED yes
    config_add_macro SDL_CPUINFO_DISABLED no
    config_add_macro SDL_EVENTS_DISABLED no
    config_add_macro SDL_FILE_DISABLED yes
    config_add_macro SDL_JOYSTICK_DISABLED yes
    config_add_macro SDL_LOADSO_DISABLED no
    config_add_macro SDL_THREADS_DISABLED no
    config_add_macro SDL_TIMERS_DISABLED no
    config_add_macro SDL_VIDEO_DISABLED no

    config_add ""
    config_add "/* Enable various shared object loading systems */"

    config_add_driver_macros DRIVERS_LOADSO SDL_LOADSO

    config_add ""
    config_add "/* Enable various threading systems */"

    config_add_driver_macros DRIVERS_THREAD SDL_THREAD

    config_add ""
    config_add "/* Enable various timer systems */"

    config_add_driver_macros DRIVERS_TIMER SDL_TIMER

    config_add ""
    config_add "/* Enable various video drivers */"

    config_add_driver_macros DRIVERS_VIDEO SDL_VIDEO_DRIVER

    config_add_driver_macros DRIVERS_MAIN SDL_MAIN

    # the following defines are good enough for all recent Unix distributions
    config_add "#define SDL_VIDEO_DRIVER_X11_DYNAMIC \"libX11.so.6\""
    config_add "#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT \"libXext.so.6\""
    config_add "#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR \"libXrandr.so.2\""
    config_add "#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XRENDER \"libXrender.so.1\""

    # todo: OpenGL support ?
    # todo: Assembly routines support ?

    config_add "#endif /* _SDL_config_h */"
}

### Build the config sub-makefile
###

make_add ()
{
    echo "$1" >> $config_mk
}

make_add_driver_macros ()
{
    local driver_list="`var_value ${1}_${HOST_OS}`"
    local driver_name
    if [ -z "$driver_list" ] ; then
        driver_list="`var_value ${1}_default`"
    fi
    for driver_name in $driver_list; do
        make_add "SDL_$2_${driver_name} := yes"
    done
}

generate_sdl_config_mk ()
{
    CFLAGS="$CFLAGS $BUILD_CFLAGS"
    LDFLAGS="$LDFLAGS $BUILD_LDFLAGS"
    create_config_mk

    make_add ""
    PWD=`pwd`
    make_add "SRC_PATH=$PWD"
    make_add "BUILD_SYSTEM=$ANDROID_QEMU_PATH/android/build"

    make_add "SDL_CONFIG_LIBC := $CONFIG_LIBC"
    make_add "SDL_CONFIG_CPUINFO := yes"

    make_add_driver_macros DRIVERS_LOADSO CONFIG_LOADSO
    make_add_driver_macros DRIVERS_THREAD CONFIG_THREAD
    make_add_driver_macros DRIVERS_TIMER  CONFIG_TIMER
    make_add_driver_macros DRIVERS_VIDEO  CONFIG_VIDEO

    make_add_driver_macros DRIVERS_MAIN   CONFIG_MAIN

    make_add "INSTALL_TARGETS := $OPTION_TARGETS"
}

### Build the final sdl-config script from the template
###

generate_sdl_config ()
{
    # build a sed script that will replace all @VARIABLES@ in the template
    # with appropriate values
    rm -f $TMPC

    # replace @exec_prefix@ with "{prefix}", and @libdir@ with "{libdir}"
    cat > $TMPC <<EOF
s!@exec_prefix@!\$\{prefix\}!g
s!@libdir@!\$\{exec_prefix\}/libs!g
s!@includedir@!\$\{prefix\}/include!g
EOF

    # we want to enable static linking, do @ENABLE_STATIC_FALSE@ and @ENABLE_STATIC_TRUE@
    cat >> $TMPC <<EOF
s!@ENABLE_STATIC_FALSE@!\#!g
s!@ENABLE_STATIC_TRUE@!!g
s!@ENABLE_SHARED_TRUE@!\#!g
s!@ENABLE_SHARED_FALSE@!!g
EOF

    # set the version to 1.2.15
    cat >> $TMPC <<EOF
s!@SDL_VERSION@!1.2.15!g
EOF

    #
    cat >> $TMPC <<EOF
s!@SDL_CFLAGS@!$SDL_CFLAGS!g
s!@SDL_STATIC_LIBS@!$SDL_STATIC_LIBS!g
EOF

    cat sdl-config.in | sed -f $TMPC > objs/sdl-config
    chmod +x objs/sdl-config
}

# copy a configuration file or perform auto-detection if one is not available
# or the --force-autodetect option was used
# $1: basename of config file
# $2: command to run to perform auto-detection/generation
#
copy_or_autodetect ()
{
    if [ "$OPTION_FORCE_AUTODETECT" != "yes" -a -f android/build/$OS/$1 ] ; then
        log "Setup      : Copying $1 from android/build/$OS"
        cp -f android/build/$OS/$1 objs/$1
    else
        log "Setup      : Auto-generating $1"
        eval $2
    fi
}

generate_all ()
{
  copy_or_autodetect SDL_config.h generate_SDL_config_h
  generate_sdl_config_mk
  copy_or_autodetect sdl-config generate_sdl_config
}

DRIVERS_LOADSO_default=DLOPEN
DRIVERS_LOADSO_darwin=DLCOMPAT
DRIVERS_LOADSO_windows=WIN32

# TODO: determine if PTHREAD_RECURSIVE or PTHREAD_RECURSIVE_NP is valid for the platform
DRIVERS_THREAD_default="PTHREAD PTHREAD_RECURSIVE_MUTEX"
DRIVERS_THREAD_linux="PTHREAD PTHREAD_RECURSIVE_MUTEX_NP"
DRIVERS_THREAD_windows=WIN32

DRIVERS_TIMER_default=UNIX
DRIVERS_TIMER_windows=WIN32

# TODO: actually compute this properly for X11 and dynamic loading !!
DRIVERS_VIDEO_default="X11 X11_DPMS X11_XINERAMA X11_XME"
DRIVERS_VIDEO_darwin="QUARTZ"
DRIVERS_VIDEO_windows="WINDIB"

DRIVERS_MAIN_default=DUMMY
DRIVERS_MAIN_darwin=MACOSX
DRIVERS_MAIN_windows=WIN32

generate_all
