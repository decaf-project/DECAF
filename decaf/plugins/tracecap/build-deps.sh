#!/bin/sh
#
#
echo building llconf...
cd $1/shared/llconf
./configure
make
echo done with llconf!

#echo building sleuthkit
#cd $1/shared/sleuthkit
#make
#echo done with sleuthkit!

