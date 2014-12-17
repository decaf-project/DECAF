#!/bin/sh
#
# A small script used to rebuild the Android goldfish kernel image
# See docs/KERNEL.TXT for usage instructions.
#
MACHINE=goldfish
VARIANT=goldfish
OUTPUT=/tmp/kernel-qemu
CROSSPREFIX=arm-eabi-
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
            CROSSTOOLCHAIN=arm-eabi-4.4.3
            CROSSPREFIX=arm-eabi-
            ZIMAGE=zImage
            ;;
        x86)
            CROSSTOOLCHAIN=i686-android-linux-4.4.3
            CROSSPREFIX=i686-android-linux-
            ZIMAGE=bzImage
            ;;
        *)
            echo "ERROR: Unsupported architecture!"
            exit 1
            ;;
    esac
    echo "Auto-config: --cross=$CROSSPREFIX"
fi

# If the cross-compiler is not in the path, try to find it automatically
CROSS_COMPILER="${CROSSPREFIX}gcc"
CROSS_COMPILER_VERSION=$($CROSS_COMPILER --version 2>/dev/null)
if [ $? != 0 ] ; then
    BUILD_TOP=$ANDROID_BUILD_TOP
    if [ -z "$BUILD_TOP" ]; then
        # Assume this script is under external/qemu/distrib/ in the
        # Android source tree.
        BUILD_TOP=$(dirname $0)/../../..
        if [ ! -d "$BUILD_TOP/prebuilt" ]; then
            BUILD_TOP=
        else
            BUILD_TOP=$(cd $BUILD_TOP && pwd)
        fi
    fi
    CROSSPREFIX=$BUILD_TOP/prebuilt/$HOST_TAG/toolchain/$CROSSTOOLCHAIN/bin/$CROSSPREFIX
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

# Do the build
#
rm -f include/asm &&
make ${CONFIG}_defconfig &&    # configure the kernel
make -j$JOBS                   # build it

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

cp -f arch/$ARCH/boot/$ZIMAGE $OUTPUT/$OUTPUT_KERNEL
cp -f vmlinux $OUTPUT/$OUTPUT_VMLINUX

echo "Kernel $CONFIG prebuilt images ($OUTPUT_KERNEL and $OUTPUT_VMLINUX) copied to $OUTPUT successfully !"
exit 0
