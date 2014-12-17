/* Copyright (C) 2008 The Android Open Source Project
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
#ifndef _ANDROID_HELP_H
#define _ANDROID_HELP_H

#include "android/utils/stralloc.h"

/* these values give the maximum length of system property
 * names and values. They must match the definitions for PROPERTY_MAX_NAME and
 * PROPERTY_MAX_VALUE in the Android source tree
 * (in system/core/include/cutils/properties.h)
 */
#define  BOOT_PROPERTY_MAX_NAME    32
#define  BOOT_PROPERTY_MAX_VALUE   92

/* appends the list of options with a small description to a dynamic string */
extern void  android_help_list_options( stralloc_t*  out );

/* output main help screen into a single dynamic string */
extern void  android_help_main( stralloc_t*  out );

/* output all help into a single dynamic string */
extern void  android_help_all( stralloc_t*  out );

/* appends the help for a given command-line option into a dynamic string
 * returns 0 on success, or -1 on error (i.e. unknown option)
 */
extern int  android_help_for_option( const char*  option, stralloc_t*  out );

/* appends the help for a given help topic into a dynamic string
 * returns 0 on success, or -1 on error (i.e. unknown topic)
 */
extern int  android_help_for_topic( const char*  topic, stralloc_t*  out );

#endif /* _ANDROID_HELP_H */
