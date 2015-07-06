#!/bin/sh
#
# A small script used to rebuild the Android goldfish kernel image
# See docs/KERNEL.TXT for usage instructions.
#
MACHINE=goldfish
VARIANT=goldfish
OUTPUT=/tmp/kernel-qemu
CROSSPREFIX=arm-linux-androideabi-
CONFIG=goldfish

# Determine the host architecture, and which default prebuilt tag we need.
# For the toolchain auto-detection.
#
HOST_OS=`uname -s`
case "$HOST_OS" in
    Darwin)
        HOST_OS=darwin
        HOST_TAG=darwin-x86
        BUILD_NUM_CPUS=$(sysctl -n hw.ncpu)
        ;;
    Linux)
        # note that building  32-bit binaries on x86_64 is handled later
        HOST_OS=linux
        HOST_TAG=linux-x86
        BUILD_NUM_CPUS=$(grep -c processor /proc/cpuinfo)
        ;;
    *)
        echo "ERROR: Unsupported OS: $HOST_OS"
        exit 1
esac

# Default number of parallel jobs during the build: cores * 2
JOBS=$(( $BUILD_NUM_CPUS * 2 ))

ARCH=arm

OPTION_HELP=no
OPTION_ARMV7=no
OPTION_OUT=
OPTION_CROSS=
OPTION_ARCH=
OPTION_CONFIG=
OPTION_JOBS=
OPTION_VERBOSE=

for opt do
    optarg=$(expr "x$opt" : 'x[^=]*=\(.*\)')
    case $opt in
    --help|-h|-\?) OPTION_HELP=yes
        ;;
    --armv7)
        OPTION_ARMV7=yes
        ;;
    --out=*)
        OPTION_OUT=$optarg
        ;;
    --cross=*)
        OPTION_CROSS=$optarg
        ;;
    --arch=*)
        OPTION_ARCH=$optarg
        ;;
    --config=*)
        OPTION_CONFIG=$optarg
        ;;
    --verbose)
        OPTION_VERBOSE=true
        ;;
    -j*)
        OPTION_JOBS=$optarg
        ;;
    *)
        echo "unknown option '$opt', use --help"
        exit 1
    esac
done

if [ $OPTION_HELP = "yes" ] ; then
    echo "Rebuild the prebuilt kernel binary for Android's emulator."
    echo ""
    echo "options (defaults are within brackets):"
    echo ""
    echo "  --help                   print this message"
    echo "  --arch=<arch>            change target architecture [$ARCH]"
    echo "  --armv7                  build ARMv7 binaries (see note below)"
    echo "  --out=<directory>        output directory [$OUTPUT]"
    echo "  --cross=<prefix>         cross-toolchain prefix [$CROSSPREFIX]"
    echo "  --config=<name>          kernel config name [$CONFIG]"
    echo "  --verbose                show build commands"
    echo "  -j<number>               launch <number> parallel build jobs [$JOBS]"
    echo ""
    echo "NOTE: --armv7 is equivalent to --config=goldfish_armv7. It is"
    echo "      ignored if --config=<name> is used."
    echo ""
    exit 0
fi

if [ -n "$OPTION_ARCH" ]; then
    ARCH=$OPTION_ARCH
fi

if [ -n "$OPTION_CONFIG" ]; then
    CONFIG=$OPTION_CONFIG
else
    if [ "$OPTION_ARMV7" = "yes" ]; then
        CONFIG=goldfish_armv7
    fi
    echo "Auto-config: --config=$CONFIG"
fi

# Check that we are in the kernel directory
if [ ! -d arch/$ARCH/mach-$MACHINE ] ; then
    echo "Cannot find arch/$ARCH/mach-$MACHINE. Please cd to the kernel source directory."
    echo "Aborting."
    #exit 1
fi

# Check output directory.
if [ -n "$OPTION_OUT" ] ; then
    if [ ! -d "$OPTION_OUT" ] ; then
        echo "Output directory '$OPTION_OUT' does not exist ! Aborting."
        exit 1
    fi
    OUTPUT=$OPTION_OUT
else
    mkdir -p $OUTPUT
fi

if [ -n "$OPTION_CROSS" ] ; then
    CROSSPREFIX="$OPTION_CROSS"
else
    case $ARCH in
        arm)
            CROSSTOOLCHAIN=arm-linux-androideabi-4.6
            CROSSPREFIX=arm-linux-androideabi-
            ;;
        x86)
            CROSSTOOLCHAIN=i686-linux-android-4.6
            CROSSPREFIX=i686-linux-android-
            ;;
        mips)
            CROSSTOOLCHAIN=mipsel-linux-android-4.6
            CROSSPREFIX=mipsel-linux-android-
            ;;
        *)
            echo "ERROR: Unsupported architecture!"
            exit 1
            ;;
    esac
    echo "Auto-config: --cross=$CROSSPREFIX"
fi

ZIMAGE=zImage

case $ARCH in
    x86)
        ZIMAGE=bzImage
        ;;
    mips)
        ZIMAGE=
        ;;
esac

# If the cross-compiler is not in the path, try to find it automatically
CROSS_COMPILER="${CROSSPREFIX}gcc"
CROSS_COMPILER_VERSION=$($CROSS_COMPILER --version 2>/dev/null)
if [ $? != 0 ] ; then
    BUILD_TOP=$ANDROID_BUILD_TOP
    if [ -z "$BUILD_TOP" ]; then
        # Assume this script is under external/qemu/distrib/ in the
        # Android source tree.
        BUILD_TOP=$(dirname $0)/../../..
        if [ ! -d "$BUILD_TOP/prebuilts" ]; then
            BUILD_TOP=
        else
            BUILD_TOP=$(cd $BUILD_TOP && pwd)
        fi
    fi
    CROSSPREFIX=$BUILD_TOP/prebuilts/gcc/$HOST_TAG/$ARCH/$CROSSTOOLCHAIN/bin/$CROSSPREFIX
    if [ "$BUILD_TOP" -a -f ${CROSSPREFIX}gcc ]; then
        echo "Auto-config: --cross=$CROSSPREFIX"
    else
        echo "It looks like $CROSS_COMPILER is not in your path ! Aborting."
        exit 1
    fi
fi

export CROSS_COMPILE="$CROSSPREFIX" ARCH SUBARCH=$ARCH

if [ "$OPTION_JOBS" ]; then
    JOBS=$OPTION_JOBS
else
    echo "Auto-config: -j$JOBS"
fi


# Special magic redirection with our magic toolbox script
# This is needed to add extra compiler flags to compiler.
# See kernel-toolchain/android-kernel-toolchain-* for details
#
export REAL_CROSS_COMPILE="$CROSS_COMPILE"
CROSS_COMPILE=$(dirname "$0")/kernel-toolchain/android-kernel-toolchain-

MAKE_FLAGS=
if [ "$OPTION_VERBOSE" ]; then
  MAKE_FLAGS="$MAKE_FLAGS V=1"
fi

# Do the build
#
rm -f include/asm &&
make ${CONFIG}_defconfig &&    # configure the kernel
make -j$JOBS $MAKE_FLAGS       # build it

if [ $? != 0 ] ; then
    echo "Could not build the kernel. Aborting !"
    exit 1
fi

# Note: The exact names of the output files are important for the Android build,
#       do not change the definitions lightly.
case $CONFIG in
    vbox*)
        OUTPUT_KERNEL=kernel-vbox
        OUTPUT_VMLINUX=vmlinux-vbox
        ;;
    goldfish)
        OUTPUT_KERNEL=kernel-qemu
        OUTPUT_VMLINUX=vmlinux-qemu
        ;;
    goldfish_armv7)
        OUTPUT_KERNEL=kernel-qemu-armv7
        OUTPUT_VMLINUX=vmlinux-qemu-armv7
        ;;
    *)
        OUTPUT_KERNEL=kernel-$CONFIG
        OUTPUT_VMLINUX=vmlinux-$CONFIG
esac

cp -f vmlinux $OUTPUT/$OUTPUT_VMLINUX
if [ ! -z $ZIMAGE ]; then
    cp -f arch/$ARCH/boot/$ZIMAGE $OUTPUT/$OUTPUT_KERNEL
else
    cp -f vmlinux $OUTPUT/$OUTPUT_KERNEL
fi
echo "Kernel $CONFIG prebuilt images ($OUTPUT_KERNEL and $OUTPUT_VMLINUX) copied to $OUTPUT successfully !"

exit 0
