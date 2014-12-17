/* Copyright (C) 2007-2008 The Android Open Source Project
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
#ifndef _android_kmsg_h
#define _android_kmsg_h

#include "qemu-common.h"

/* this chardriver is used to read the kernel messages coming
 * from the first serial port (i.e. /dev/ttyS0) and store them
 * in memory for later...
 */

typedef enum {
    ANDROID_KMSG_SAVE_MESSAGES  = (1 << 0),
    ANDROID_KMSG_PRINT_MESSAGES = (1 << 1),
} AndroidKmsgFlags;

extern void  android_kmsg_init( AndroidKmsgFlags  flags );

extern CharDriverState*  android_kmsg_get_cs( void );

#endif /* _android_kmsg_h */
