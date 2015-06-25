/* Copyright (C) 2011 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#ifndef _ANDROID_AVD_UTIL_H
#define _ANDROID_AVD_UTIL_H

/* A collection of simple functions to extract relevant AVD-related
 * information either from an SDK AVD or a platform build.
 */

/* Return the path to the Android SDK root installation.
 *
 * (*pFromEnv) will be set to 1 if it comes from the $ANDROID_SDK_ROOT
 * environment variable, or 0 otherwise.
 *
 * Caller must free() returned string.
 */
char* path_getSdkRoot( char *pFromEnv );

/* Return the path to the AVD's root configuration .ini file. it is located in
 * ~/.android/avd/<name>.ini or Windows equivalent
 *
 * This file contains the path to the AVD's content directory, which
 * includes its own config.ini.
 */
char* path_getRootIniPath( const char*  avdName );

/* Return the target architecture for a given AVD.
 * Called must free() returned string.
 */
char* path_getAvdTargetArch( const char* avdName );

/* Retrieves a string corresponding to the target architecture
 * when in the Android platform tree. The only way to do that
 * properly for now is to look at $OUT/system/build.prop:
 *
 *   ro.product.cpu-abi=<abi>
 *
 * Where <abi> can be 'armeabi', 'armeabi-v7a' or 'x86'.
 */
char* path_getBuildTargetArch( const char* androidOut );

/* Retrieves a string corresponding to the target CPU ABI
 * when in the Android platform tree. The only way to do that
 * properly for now is to look at $OUT/system/build.prop:
 *
 *   ro.product.cpu-abi=<abi>
 *
 * Where <abi> can be 'armeabi', 'armeabi-v7a' or 'x86'.
 */
char* path_getBuildTargetAbi( const char* androidOut );

/* Retrieve the target API level when in the Android platform tree.
 * This can be a very large number like 1000 if the value cannot
 * be extracted from the appropriate file
 */
int path_getBuildTargetApiLevel( const char* androidOut );

/* Returns mode in which ADB daemon running in the guest communicates with the
 * emulator
 * Return:
 *  0 - ADBD communicates with the emulator via forwarded TCP port 5555 (a
 *      "legacy" mode).
 *  1 - ADBD communicates with the emulator via 'adb' QEMUD service.
 */
int path_getAdbdCommunicationMode( const char* androidOut );

#endif /* _ANDROID_AVD_UTIL_H */
