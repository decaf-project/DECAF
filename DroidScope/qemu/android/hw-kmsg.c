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
#include "android/hw-kmsg.h"
#include "qemu-char.h"
#include "charpipe.h"
#include "android/utils/debug.h"

static CharDriverState*  android_kmsg_cs;

typedef struct {
    CharDriverState*  cs;
    AndroidKmsgFlags  flags;
} KernelLog;

static int
kernel_log_can_read( void*  opaque )
{
    return 1024;
}

static void
kernel_log_read( void*  opaque, const uint8_t*  from, int  len )
{
    KernelLog*  k = opaque;

    if (k->flags & ANDROID_KMSG_PRINT_MESSAGES)
        printf( "%.*s", len, (const char*)from );

    /* XXXX: TODO: save messages into in-memory buffer for later retrieval */
}

static void
kernel_log_init( KernelLog*  k, AndroidKmsgFlags  flags )
{
    if ( qemu_chr_open_charpipe( &k->cs, &android_kmsg_cs ) < 0 ) {
        derror( "could not create kernel log charpipe" );
        exit(1);
    }

    qemu_chr_add_handlers( k->cs, kernel_log_can_read, kernel_log_read, NULL, k );

    k->flags = flags;
}

static KernelLog  _kernel_log[1];

void
android_kmsg_init( AndroidKmsgFlags  flags )
{
    if (_kernel_log->cs == NULL)
        kernel_log_init( _kernel_log, flags );
}


CharDriverState*  android_kmsg_get_cs( void )
{
    if (android_kmsg_cs == NULL) {
        android_kmsg_init(0);
    }
    return android_kmsg_cs;
}
