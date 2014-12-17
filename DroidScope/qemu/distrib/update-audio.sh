#!/bin/bash
#
# this script is used to update the prebuilt libqemu-audio.a file in the Android source tree
# we use a prebuilt package because we don't want to force the installation of the ALSA / EsounD / Whatever
# development packages on every developer machine, or every build server.
#

# assumes this script is located in the 'distrib' sub-directory
cd `dirname $0`
cd ..
. android/build/common.sh

check_android_build
if [ $IN_ANDROID_BUILD != yes ] ; then
    echo "Sorry, this script can only be run from a full Android build tree"
    exit 1
fi

force_32bit_binaries
locate_android_prebuilt

# find the prebuilt directory
OS=`uname -s`
EXE=""
case "$OS" in
    Darwin)
        CPU=`uname -p`
        if [ "$CPU" == "i386" ] ; then
            OS=darwin-x86
        else
            OS=darwin-ppc
        fi
        ;;
    Linux)
        CPU=`uname -m`
        case "$CPU" in
        i?86|x86_64|amd64)
            CPU=x86
            ;;
        esac
        OS=linux-$CPU
        ;;
    *_NT-*)
        OS=windows
        EXE=.exe
        ;;
esac

PREBUILT=$(locate_depot_files //branches/cupcake/android/prebuilt/$OS)

# find the GNU Make program
is_gnu_make ()
{
    version=$($1 -v | grep GNU)
    if test -n "$version"; then
        echo "$1"
    else
        echo ""
    fi
}

if test -z "$GNUMAKE"; then
    GNUMAKE=`which make` && GNUMAKE=$(is_gnu_make $GNUMAKE)
fi

if test -z "$GNUMAKE"; then
    GNUMAKE=`which gmake` && GNUMAKE=$(is_gnu_make $GNUMAKE)
fi

if test -z "$GNUMAKE"; then
    echo "could not find GNU Make on this machine. please define GNUMAKE to point to it"
    exit 3
fi

TEST=$(is_gnu_make $GNUMAKE)
if test -z "$TEST"; then
    echo "it seems that '$GNUMAKE' is not a working GNU Make binary. please check the definition of GNUMAKE"
    exit 3
fi

# ensure we have a recent audio library built
#
#echo "GNUMAKE is $GNUMAKE"
source=objs/libqemu-audio.a
./android-configure.sh
$GNUMAKE $source BUILD_QEMU_AUDIO_LIB=true || (echo "could not build the audio library. Aborting" && exit 1)

# now do a p4 edit, a copy and ask for submission
#
TARGET=$ANDROID_PREBUILT/emulator/libqemu-audio.a

cp -f $source $TARGET
echo "ok, file copied to $TARGET"

