#!/bin/bash
#
# this script is used to rebuild all QEMU binaries for the host
# platforms.
#
# assume that the device tree is in TOP
#

case $(uname -s) in
    Linux)
        HOST_NUM_CPUS=`cat /proc/cpuinfo | grep processor | wc -l`
        ;;
    Darwin|FreeBsd)
        HOST_NUM_CPUS=`sysctl -n hw.ncpu`
        ;;
    CYGWIN*|*_NT-*)
        HOST_NUM_CPUS=$NUMBER_OF_PROCESSORS
        ;;
    *)  # let's play safe here
        HOST_NUM_CPUS=1
esac

cd `dirname $0`
rm -rf objs &&
./android-configure.sh $@ &&
make -j$HOST_NUM_CPUS &&
echo "Done. !!"
