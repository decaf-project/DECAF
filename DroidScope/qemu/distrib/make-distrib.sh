#!/bin/bash
#
# this script is used to build a source distribution package for the Android emulator
# the package includes:
#  - the sources of our patched SDL library
#  - the sources of our patched QEMU emulator
#  - appropriate scripts to rebuild the emulator binary
#

# get absolute path of source directory tree
CURDIR=`dirname $0`
TOPDIR=`cd $CURDIR/.. && pwd`

# create temporary directory
TMPROOT=/tmp/android-package
DATE=$(date +%Y%m%d)
PACKAGE=android-emulator-$DATE
TMPDIR=$TMPROOT/$PACKAGE
if ! ( rm -rf $TMPROOT && mkdir -p $TMPDIR ) then
    echo "could not create temporary directory $TMPDIR"
    exit 3
fi

# clone the current source tree to $TMPDIR/qemu
QEMUDIR=$TMPDIR/qemu
echo "Copying sources to $QEMUDIR"
cd $TMPDIR && git clone file://$TOPDIR $QEMUDIR && rm -rf $QEMUDIR/.git
if [ $? != 0 ] ; then
    echo "Could not clone sources"
fi

echo "copying control scripts"
mv $QEMUDIR/distrib/build-emulator.sh $TMPDIR/build-emulator.sh
mv $QEMUDIR/distrib/README $TMPDIR/README

echo "packaging release into a tarball"
cd $TMPROOT
tar cjf $PACKAGE.tar.bz2 $PACKAGE

echo "cleaning up"
rm -rf $TMPDIR

echo "please grab $TMPROOT/$PACKAGE.tar.bz2"
