#!/bin/sh
grep AFFLIB_VERSION lib/afflib.h | grep -v AF_ | awk '{print $3}'
