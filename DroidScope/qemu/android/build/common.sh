# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# This file is included by other shell scripts; do not execute it directly.
# It contains common definitions.
#
PROGNAME=`basename $0`

## Logging support
##
VERBOSE=yes
VERBOSE2=no

log ()
{
    if [ "$VERBOSE" = "yes" ] ; then
        echo "$1"
    fi
}

log2 ()
{
    if [ "$VERBOSE2" = "yes" ] ; then
        echo "$1"
    fi
}

## Utilities
##

# return the value of a given named variable
# $1: variable name
#
var_value ()
{
    # find a better way to do that ?
    local result
    eval result="$`echo $1`"
    echo $result
}

# convert to uppercase
to_uppercase ()
{
    echo $1 | tr "[:lower:]" "[:upper:]"
}

## Normalize OS and CPU
##

CPU=`uname -m`
case "$CPU" in
    i?86) CPU=x86
    ;;
    amd64) CPU=x86_64
    ;;
    powerpc) CPU=ppc
    ;;
esac

log2 "CPU=$CPU"

# at this point, the supported values for CPU are:
#   x86
#   x86_64
#   ppc
#
# other values may be possible but haven't been tested
#

EXE=""
OS=`uname -s`
case "$OS" in
    Darwin)
        OS=darwin-$CPU
        ;;
    Linux)
        # note that building  32-bit binaries on x86_64 is handled later
        OS=linux-$CPU
	;;
    FreeBSD)
        OS=freebsd-$CPU
        ;;
    CYGWIN*|*_NT-*)
        OS=windows
        EXE=.exe
        if [ "x$OSTYPE" = xcygwin ] ; then
            OS=cygwin
            HOST_CFLAGS="$CFLAGS -mno-cygwin"
            HOST_LDFLAGS="$LDFLAGS -mno-cygwin"
        fi
        ;;
esac

log2 "OS=$OS"
log2 "EXE=$EXE"

# at this point, the value of OS should be one of the following:
#   linux-x86
#   linux-x86_64
#   darwin-x86
#   darwin-ppc
#   windows  (MSys)
#   cygwin
#
# Note that cygwin is treated as a special case because it behaves very differently
# for a few things
#
# other values may be possible but have not been tested

# define HOST_OS as $OS without any cpu-specific suffix
#
case $OS in
    linux-*) HOST_OS=linux
    ;;
    darwin-*) HOST_OS=darwin
    ;;
    freebsd-*) HOST_OS=freebsd
    ;;
    *) HOST_OS=$OS
esac

# define HOST_ARCH as the $CPU
HOST_ARCH=$CPU

# define HOST_TAG
# special case: windows-x86 => windows
compute_host_tag ()
{
    case $HOST_OS-$HOST_ARCH in
        cygwin-x86|windows-x86)
            HOST_TAG=windows
            ;;
        *)
            HOST_TAG=$HOST_OS-$HOST_ARCH
            ;;
    esac
}
compute_host_tag

#### Toolchain support
####

# Various probes are going to need to run a small C program
TMPC=/tmp/android-$$-test.c
TMPO=/tmp/android-$$-test.o
TMPE=/tmp/android-$$-test$EXE
TMPL=/tmp/android-$$-test.log

# cleanup temporary files
clean_temp ()
{
    rm -f $TMPC $TMPO $TMPL $TMPE
}

# cleanup temp files then exit with an error
clean_exit ()
{
    clean_temp
    exit 1
}

# this function should be called to enforce the build of 32-bit binaries on 64-bit systems
# that support it.
FORCE_32BIT=no
force_32bit_binaries ()
{
    if [ $CPU = x86_64 ] ; then
        FORCE_32BIT=yes
        case $OS in
            linux-x86_64) OS=linux-x86 ;;
            darwin-x86_64) OS=darwin-x86 ;;
	    freebsd-x86_64) OS=freebsd-x86 ;;
        esac
        HOST_ARCH=x86
        CPU=x86
        compute_host_tag
        log "Check32Bits: Forcing generation of 32-bit binaries (--try-64 to disable)"
    fi
}

# Enable linux-mingw32 compilation. This allows you to build
# windows executables on a Linux machine, which is considerably
# faster than using Cygwin / MSys to do the same job.
#
enable_linux_mingw ()
{
    # Are we on Linux ?
    log "Mingw      : Checking for Linux host"
    if [ "$HOST_OS" != "linux" ] ; then
        echo "Sorry, but mingw compilation is only supported on Linux !"
        exit 1
    fi
    # Do we have the binaries installed
    log "Mingw      : Checking for mingw32 installation"
    MINGW32_PREFIX=i586-mingw32msvc
    find_program MINGW32_CC $MINGW32_PREFIX-gcc
    if [ -z "$MINGW32_CC" ] ; then
        echo "ERROR: It looks like $MINGW32_PREFIX-gcc is not in your path"
        echo "Please install the mingw32 package !"
        exit 1
    fi
    log2 "Mingw      : Found $MINGW32_CC"
    CC=$MINGW32_CC
    LD=$MINGW32_CC
    AR=$MINGW32_PREFIX-ar
    FORCE_32BIT=no
}

# Cygwin is normally not supported, unless you call this function
#
enable_cygwin ()
{
    if [ $OS = cygwin ] ; then
        CFLAGS="$CFLAGS -mno-cygwin"
        LDFLAGS="$LDFLAGS -mno-cygwin"
        OS=windows
        HOST_OS=windows
    fi
}

# this function will setup the compiler and linker and check that they work as advertized
# note that you should call 'force_32bit_binaries' before this one if you want it to work
# as advertized.
#
setup_toolchain ()
{
    if [ "$OS" = cygwin ] ; then
        echo "Do not compile this program or library with Cygwin, use MSYS instead !!"
        echo "As an experimental feature, you can try to --try-cygwin option to override this"
        exit 2
    fi

    if [ -z "$CC" ] ; then
        CC=gcc
        if [ $CPU = "powerpc" ] ; then
            CC=gcc-3.3
        fi
    fi

    # check that we can compile a trivial C program with this compiler
    cat > $TMPC <<EOF
int main(void) {}
EOF

    if [ $FORCE_32BIT = yes ] ; then
        CFLAGS="$CFLAGS -m32"
        LDFLAGS="$LDFLAGS -m32"
        compile
        if [ $? != 0 ] ; then
            # sometimes, we need to also tell the assembler to generate 32-bit binaries
            # this is highly dependent on your GCC installation (and no, we can't set
            # this flag all the time)
            CFLAGS="$CFLAGS -Wa,--32"
            compile
        fi
    fi

    compile
    if [ $? != 0 ] ; then
        echo "your C compiler doesn't seem to work: $CC"
        cat $TMPL
        clean_exit
    fi
    log "CC         : compiler check ok ($CC)"

    # check that we can link the trivial program into an executable
    if [ -z "$LD" ] ; then
        LD=$CC
    fi
    link
    if [ $? != 0 ] ; then
        OLD_LD=$LD
        LD=gcc
        compile
        link
        if [ $? != 0 ] ; then
            LD=$OLD_LD
            echo "your linker doesn't seem to work:"
            cat $TMPL
            clean_exit
        fi
    fi
    log "LD         : linker check ok ($LD)"

    if [ -z "$AR" ]; then
        AR=ar
    fi
    log "AR         : archiver ($AR)"
}

# try to compile the current source file in $TMPC into an object
# stores the error log into $TMPL
#
compile ()
{
    log2 "Object     : $CC -o $TMPO -c $CFLAGS $TMPC"
    $CC -o $TMPO -c $CFLAGS $TMPC 2> $TMPL
}

# try to link the recently built file into an executable. error log in $TMPL
#
link()
{
    log2 "Link      : $LD -o $TMPE $TMPO $LDFLAGS"
    $LD -o $TMPE $TMPO $LDFLAGS 2> $TMPL
}

# run a command
#
execute()
{
    log2 "Running: $*"
    $*
}

# perform a simple compile / link / run of the source file in $TMPC
compile_exec_run()
{
    log2 "RunExec    : $CC -o $TMPE $CFLAGS $TMPC"
    compile
    if [ $? != 0 ] ; then
        echo "Failure to compile test program"
        cat $TMPC
        cat $TMPL
        clean_exit
    fi
    link
    if [ $? != 0 ] ; then
        echo "Failure to link test program"
        cat $TMPC
        echo "------"
        cat $TMPL
        clean_exit
    fi
    $TMPE
}

## Feature test support
##

# Each feature test allows us to check against a single target-specific feature
# We run the feature checks in a Makefile in order to be able to do them in
# parallel, and we also have some cached values in our output directory, just
# in case.
#
# check that a given C program in $TMPC can be compiled on the host system
# $1: variable name which will be set to "yes" or "no" depending on result
# you can define EXTRA_CFLAGS for extra C compiler flags
# for convenience, this variable will be unset by the function
#
feature_check_compile ()
{
    local result_cc=yes
    local OLD_CFLAGS
    OLD_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $EXTRA_CFLAGS"
    compile
    if [ $? != 0 ] ; then
        result_cc=no
    fi
    eval $1=$result_cc
    EXTRA_CFLAGS=
    CFLAGS=$OLD_CFLAGS
}

# check that a given C program $TMPC can be linked on the host system
# $1: variable name which will be set to "yes" or "no" depending on result
# you can define EXTRA_CFLAGS for extra C compiler flags
# you can define EXTRA_LDFLAGS for extra linker flags
# for convenience, these variables will be unset by the function
#
feature_check_link ()
{
    local result_cl=yes
    local OLD_CFLAGS OLD_LDFLAGS
    OLD_CFLAGS=$CFLAGS
    OLD_LDFLAGS=$LDFLAGS
    CFLAGS="$CFLAGS $EXTRA_CFLAGS"
    LDFLAGS="$LDFLAGS $EXTRA_LDFLAGS"
    compile
    if [ $? != 0 ] ; then
        result_cl=no
    else
        link
        if [ $? != 0 ] ; then
            result_cl=no
        fi
    fi
    CFLAGS=$OLD_CFLAGS
    LDFLAGS=$OLD_LDFLAGS
    eval $1=$result_cl
}

# check that a given C header file exists on the host system
# $1: variable name which will be set to "yes" or "no" depending on result
# $2: header name
#
# you can define EXTRA_CFLAGS for extra C compiler flags
# for convenience, this variable will be unset by the function.
#
feature_check_header ()
{
    local result_ch
    log "HeaderCheck: $2"
    echo "#include $2" > $TMPC
    cat >> $TMPC <<EOF
        int main(void) { return 0; }
EOF
    feature_check_compile result_ch
    eval $1=$result_ch
    #eval result=$`echo $1`
    #log  "Host       : $1=$result_ch"
}

# run the test program that is in $TMPC and set its exit status
# in the $1 variable.
# you can define EXTRA_CFLAGS and EXTRA_LDFLAGS
#
feature_run_exec ()
{
    local run_exec_result
    local OLD_CFLAGS="$CFLAGS"
    local OLD_LDFLAGS="$LDFLAGS"
    CFLAGS="$CFLAGS $EXTRA_CFLAGS"
    LDFLAGS="$LDFLAGS $EXTRA_LDFLAGS"
    compile_exec_run
    run_exec_result=$?
    CFLAGS="$OLD_CFLAGS"
    LDFLAGS="$OLD_LDFLAGS"
    eval $1=$run_exec_result
    log "Host       : $1=$run_exec_result"
}

## Android build system auto-detection
##

# check whether we're running within the Android build system
# sets the variable IN_ANDROID_BUILD to either "yes" or "no"
#
# in case of success, defines ANDROID_TOP to point to the top
# of the Android source tree.
#
check_android_build ()
{
    unset ANDROID_TOP
    IN_ANDROID_BUILD=no

    if [ -z "$ANDROID_BUILD_TOP" ] ; then
        return ;
    fi

    ANDROID_TOP=$ANDROID_BUILD_TOP
    log "ANDROID_TOP found at $ANDROID_TOP"
    # $ANDROID_TOP/config/envsetup.make is for the old tree layout
    # $ANDROID_TOP/build/envsetup.sh is for the new one
    ANDROID_CONFIG_MK=$ANDROID_TOP/build/core/config.mk
    if [ ! -f $ANDROID_CONFIG_MK ] ; then
        ANDROID_CONFIG_MK=$ANDROID_TOP/config/envsetup.make
    fi
    if [ ! -f $ANDROID_CONFIG_MK ] ; then
        echo "Weird: Cannot find build system root defaulting to non-Android build"
        unset ANDROID_TOP
        return
    fi
    # normalize ANDROID_TOP, we don't want a trailing /
    ANDROID_TOPDIR=`dirname $ANDROID_TOP`
    if [ "$ANDROID_TOPDIR" != "." ] ; then
        ANDROID_TOP=$ANDROID_TOPDIR/`basename $ANDROID_TOP`
    fi
    IN_ANDROID_BUILD=yes
}

# Get the value of an Android build variable as an absolute path.
# you should only call this if IN_ANDROID_BUILD is "yes"
#
get_android_abs_build_var ()
{
   (cd $ANDROID_TOP && CALLED_FROM_SETUP=true BUILD_SYSTEM=build/core make -f $ANDROID_CONFIG_MK dumpvar-abs-$1)
}

# Locate the Android prebuilt directory for your os
# you should only call this if IN_ANDROID_BUILD is "yes"
#
# This will set ANDROID_PREBUILT_HOST_TAG, ANDROID_PREBUILT and ANDROID_PREBUILTS
#
locate_android_prebuilt ()
{
    # locate prebuilt directory
    ANDROID_PREBUILT_HOST_TAG=$OS
    ANDROID_PREBUILT=$ANDROID_TOP/prebuilt/$ANDROID_PREBUILT_HOST_TAG  # AOSP still has it
    ANDROID_PREBUILTS=$ANDROID_TOP/prebuilts/misc/$ANDROID_PREBUILT_HOST_TAG # AOSP does't have it yet
    if [ ! -d $ANDROID_PREBUILT ] ; then
        # this can happen when building on x86_64, or in AOSP
        case $OS in
            linux-x86_64)
                ANDROID_PREBUILT_HOST_TAG=linux-x86
                ANDROID_PREBUILT=$ANDROID_TOP/prebuilt/$ANDROID_PREBUILT_HOST_TAG
                ;;
            *)
        esac
        if [ ! -d $ANDROID_PREBUILT ] ; then
            ANDROID_PREBUILT=
        fi
    fi
    if [ ! -d $ANDROID_PREBUILTS ] ; then
        # this can happen when building on x86_64
        case $OS in
            linux-x86_64)
                ANDROID_PREBUILT_HOST_TAG=linux-x86
                ANDROID_PREBUILTS=$ANDROID_TOP/prebuilts/misc/$ANDROID_PREBUILT_HOST_TAG
                ;;
            *)
        esac
        if [ ! -d $ANDROID_PREBUILTS ] ; then
            ANDROID_PREBUILTS=
        fi
    fi
    log "Prebuilt   : ANDROID_PREBUILT=$ANDROID_PREBUILT"
    log "Prebuilts  : ANDROID_PREBUILTS=$ANDROID_PREBUILTS"
}

## Build configuration file support
## you must define $config_mk before calling this function
##
create_config_mk ()
{
    # create the directory if needed
    local  config_dir
    config_mk=${config_mk:-objs/config.make}
    config_dir=`dirname $config_mk`
    mkdir -p $config_dir 2> $TMPL
    if [ $? != 0 ] ; then
        echo "Can't create directory for build config file: $config_dir"
        cat $TMPL
        clean_exit
    fi

    # re-create the start of the configuration file
    log "Generate   : $config_mk"

    echo "# This file was autogenerated by $PROGNAME. Do not edit !" > $config_mk
    echo "OS          := $OS" >> $config_mk
    echo "HOST_OS     := $HOST_OS" >> $config_mk
    echo "HOST_ARCH   := $HOST_ARCH" >> $config_mk
    echo "CC          := $CC" >> $config_mk
    echo "HOST_CC     := $CC" >> $config_mk
    echo "LD          := $LD" >> $config_mk
    echo "AR          := $AR" >> $config_mk
    echo "CFLAGS      := $CFLAGS" >> $config_mk
    echo "LDFLAGS     := $LDFLAGS" >> $config_mk
}

add_android_config_mk ()
{
    echo "" >> $config_mk
    if [ $TARGET_ARCH = arm ] ; then
    echo "TARGET_ARCH       := arm" >> $config_mk
    fi
    if [ $TARGET_ARCH = x86 ] ; then
    echo "TARGET_ARCH       := x86" >> $config_mk
    fi
    echo "HOST_PREBUILT_TAG := $HOST_TAG" >> $config_mk
    echo "PREBUILT          := $ANDROID_PREBUILT" >> $config_mk
    echo "PREBUILTS         := $ANDROID_PREBUILTS" >> $config_mk
}

# Find pattern $1 in string $2
# This is to be used in if statements as in:
#
#    if pattern_match <pattern> <string>; then
#       ...
#    fi
#
pattern_match ()
{
    echo "$2" | grep -q -E -e "$1"
}

# Find if a given shell program is available.
# We need to take care of the fact that the 'which <foo>' command
# may return either an empty string (Linux) or something like
# "no <foo> in ..." (Darwin). Also, we need to redirect stderr
# to /dev/null for Cygwin
#
# $1: variable name
# $2: program name
#
# Result: set $1 to the full path of the corresponding command
#         or to the empty/undefined string if not available
#
find_program ()
{
    local PROG
    PROG=`which $2 2>/dev/null`
    if [ -n "$PROG" ] ; then
        if pattern_match '^no ' "$PROG"; then
            PROG=
        fi
    fi
    eval $1="$PROG"
}
